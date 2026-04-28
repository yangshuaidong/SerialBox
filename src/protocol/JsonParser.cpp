/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/protocol/JsonParser.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/protocol/JsonParser.h"
#include <QJsonParseError>
#include <QJsonObject>

JsonParser::JsonParser(QObject *parent)
    : IProtocolParser(parent)
{
}

bool JsonParser::match(const QByteArray &buffer)
{
    // 简单检测：以 { 开头，找对应的 }
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
            if (depth == 0) return true;  // 找到完整 JSON 对象
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

    // 转为 QVariantMap
    result.fields = doc.object().toVariantMap();
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
