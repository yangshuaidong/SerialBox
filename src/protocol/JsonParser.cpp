#include "SerialBox/protocol/JsonParser.h"
#include <QJsonParseError>
#include <QJsonObject>

JsonParser::JsonParser(QObject *parent)
    : IProtocolParser(parent)
{
}

bool JsonParser::match(const QByteArray &buffer)
{
    // 检测完整 JSON 对象：以 { 开头，深度匹配到 }
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

    if (!match(buffer)) return result;

    // 提取 JSON 帧
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

    // 区分 JSON-RPC 2.0 与普通 JSON
    QJsonObject obj = doc.object();
    if (obj.contains("jsonrpc") && obj.value("jsonrpc").toString() == "2.0") {
        result.fields["_protocol"] = "jsonrpc2";
        if (obj.contains("method")) {
            // Request 或 Notification
            result.fields["_rpcType"] = obj.contains("id") ? "request" : "notification";
            result.fields["_rpcMethod"] = obj.value("method").toString();
        } else if (obj.contains("result")) {
            result.fields["_rpcType"] = "response";
        } else if (obj.contains("error")) {
            result.fields["_rpcType"] = "error";
            QJsonObject errObj = obj.value("error").toObject();
            result.fields["_rpcErrorCode"] = errObj.value("code").toInt();
            result.fields["_rpcErrorMsg"] = errObj.value("message").toString();
        }
    }

    result.fields.unite(obj.toVariantMap());
    result.displayText = QString(QJsonDocument(doc).toJson(QJsonDocument::Compact));

    return result;
}

QByteArray JsonParser::build(const QVariantMap &fields)
{
    QJsonObject obj = QJsonObject::fromVariantMap(fields);
    // 移除内部标记字段
    obj.remove("_protocol");
    obj.remove("_rpcType");
    obj.remove("_rpcMethod");
    obj.remove("_rpcErrorCode");
    obj.remove("_rpcErrorMsg");
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString JsonParser::prettyPrint(const QJsonDocument &doc)
{
    return doc.toJson(QJsonDocument::Indented);
}
