/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/protocol/CustomFrameParser.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/protocol/CustomFrameParser.h"

CustomFrameParser::CustomFrameParser(QObject *parent)
    : IProtocolParser(parent)
{
}

void CustomFrameParser::setFrameHeader(const QByteArray &header) { m_frameHeader = header; }
void CustomFrameParser::setLengthOffset(int offset, int size) {
    m_lengthOffset = offset;
    m_lengthSize = size;
}
void CustomFrameParser::setCrcEnabled(bool enable) { m_crcEnabled = enable; }

bool CustomFrameParser::match(const QByteArray &buffer)
{
    if (buffer.size() < m_frameHeader.size() + m_lengthOffset + m_lengthSize) return false;
    if (!buffer.startsWith(m_frameHeader)) return false;

    // 读取长度字段
    int payloadLen = 0;
    for (int i = 0; i < m_lengthSize; ++i) {
        payloadLen = (payloadLen << 8) | static_cast<quint8>(buffer[m_frameHeader.size() + m_lengthOffset + i]);
    }

    int totalLen = m_frameHeader.size() + m_lengthOffset + m_lengthSize + payloadLen
                 + (m_crcEnabled ? 2 : 0);
    return buffer.size() >= totalLen;
}

IProtocolParser::ParseResult CustomFrameParser::parse(QByteArray &buffer)
{
    ParseResult result;
    result.protocolName = name();

    if (!match(buffer)) return result;

    int payloadLen = 0;
    int lenFieldPos = m_frameHeader.size() + m_lengthOffset;
    for (int i = 0; i < m_lengthSize; ++i) {
        payloadLen = (payloadLen << 8) | static_cast<quint8>(buffer[lenFieldPos + i]);
    }

    int totalLen = m_frameHeader.size() + m_lengthOffset + m_lengthSize + payloadLen
                 + (m_crcEnabled ? 2 : 0);

    QByteArray frame = buffer.left(totalLen);
    QByteArray payload = frame.mid(m_frameHeader.size() + m_lengthOffset + m_lengthSize, payloadLen);

    result.matched = true;
    result.rawFrames.append(frame);
    result.fields["header"]  = frame.left(m_frameHeader.size()).toHex(' ').toUpper();
    result.fields["length"]  = payloadLen;
    result.fields["payload"] = QString(payload.toHex(' ').toUpper());
    result.fields["displayPayload"] = payload;

    result.displayText = QString("[自定义帧] 头:%1 长:%2 数据:%3")
        .arg(result.fields["header"].toString())
        .arg(payloadLen)
        .arg(result.fields["payload"].toString());

    buffer.remove(0, totalLen);
    return result;
}

QByteArray CustomFrameParser::build(const QVariantMap &fields)
{
    QByteArray payload = fields.value("payload").toByteArray();
    QByteArray frame = m_frameHeader;

    // 写入长度字段
    int len = payload.size();
    for (int i = m_lengthSize - 1; i >= 0; --i) {
        frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
    }

    frame.append(payload);

    if (m_crcEnabled) {
        // 简单 CRC16
        quint16 crc = 0;
        for (auto b : frame) crc += static_cast<quint8>(b);
        frame.append(static_cast<char>(crc >> 8));
        frame.append(static_cast<char>(crc & 0xFF));
    }

    return frame;
}
