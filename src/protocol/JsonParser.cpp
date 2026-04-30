#include "SerialBox/protocol/JsonParser.h"
#include <QJsonParseError>
#include <QJsonObject>

JsonParser::JsonParser(QObject *parent)
    : IProtocolParser(parent)
{
}

bool JsonParser::match(const QByteArray &buffer)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (int i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && inString) { escaped = true; continue; }
        if (c == '"') { inString = !inString; continue; }
        if (inString) continue;

        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) return true;
        }
    }
    return false;
}

IProtocolParser::ParseResult JsonParser::parse(QByteArray &buffer)
{
    ParseResult result;
    result.protocolName = name();

    // 一次扫描：同时完成 match 判断和 frameEnd 提取
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    int frameEnd = -1;

    for (int i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && inString) { escaped = true; continue; }
        if (c == '"') { inString = !inString; continue; }
        if (inString) continue;

        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) { frameEnd = i + 1; break; }
        }
    }

    if (frameEnd <= 0) return result;

    QByteArray jsonFrame = buffer.left(frameEnd);
    buffer.remove(0, frameEnd);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFrame, &err);

    if (err.error != QJsonParseError::NoError) {
        result.displayText = QString("[JSON 解析错误] %1").arg(err.errorString());
        return result;
    }

    result.matched = true;
    result.rawFrames.append(jsonFrame);

    // 区分 JSON-RPC 2.0 与普通 JSON（元数据写入 rpcMeta，不污染 fields）
    QJsonObject obj = doc.object();
    if (obj.contains("jsonrpc") && obj.value("jsonrpc").toString() == "2.0") {
        result.rpcMeta.isRpc = true;
        if (obj.contains("method")) {
            result.rpcMeta.type = obj.contains("id") ? "request" : "notification";
            result.rpcMeta.method = obj.value("method").toString();
        } else if (obj.contains("result")) {
            result.rpcMeta.type = "response";
        } else if (obj.contains("error")) {
            result.rpcMeta.type = "error";
            QJsonObject errObj = obj.value("error").toObject();
            result.rpcMeta.errorCode = errObj.value("code").toInt();
            result.rpcMeta.errorMsg = errObj.value("message").toString();
        }
    }

    result.fields = obj.toVariantMap();
    result.displayText = QString(QJsonDocument(doc).toJson(QJsonDocument::Compact));

    return result;
}

QByteArray JsonParser::build(const QVariantMap &fields)
{
    QJsonObject obj = QJsonObject::fromVariantMap(fields);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString JsonParser::prettyPrint(const QJsonDocument &doc)
{
    return doc.toJson(QJsonDocument::Indented);
}
