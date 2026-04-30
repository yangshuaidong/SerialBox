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

/**
 * CRC-16/CCITT (poly=0x1021, init=0xFFFF)
 * 替换原先的简单累加校验，提供工业级检错能力
 */
static quint16 crc16ccitt(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (auto byte : data) {
        crc ^= static_cast<quint8>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

bool CustomFrameParser::match(const QByteArray &buffer)
{
    int minLen = m_frameHeader.size() + m_lengthOffset + m_lengthSize;
    if (buffer.size() < minLen) return false;
    if (!buffer.startsWith(m_frameHeader)) return false;

    // 读取长度字段
    int payloadLen = 0;
    for (int i = 0; i < m_lengthSize; ++i) {
        payloadLen = (payloadLen << 8) | static_cast<quint8>(buffer[m_frameHeader.size() + m_lengthOffset + i]);
    }

    int totalLen = m_frameHeader.size() + m_lengthOffset + m_lengthSize + payloadLen
                 + (m_crcEnabled ? 2 : 0);
    if (buffer.size() < totalLen) return false;

    // ★ CRC 校验
    if (m_crcEnabled) {
        QByteArray framePart = buffer.left(totalLen - 2);
        quint16 calcCrc = crc16ccitt(framePart);
        quint16 recvCrc = static_cast<quint8>(buffer[totalLen - 2]) << 8
                        | static_cast<quint8>(buffer[totalLen - 1]);
        return calcCrc == recvCrc;
    }

    return true;
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

    if (m_crcEnabled) {
        quint16 calcCrc = crc16ccitt(frame.left(totalLen - 2));
        quint16 recvCrc = static_cast<quint8>(frame[totalLen - 2]) << 8
                        | static_cast<quint8>(frame[totalLen - 1]);
        result.fields["crcOk"] = (calcCrc == recvCrc);
    }

    result.displayText = QString("[自定义帧] 头:%1 长:%2 数据:%3%4")
        .arg(result.fields["header"].toString())
        .arg(payloadLen)
        .arg(result.fields["payload"].toString())
        .arg(m_crcEnabled ? (result.fields.value("crcOk").toBool() ? " CRC:✓" : " CRC:✗") : "");

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
        quint16 crc = crc16ccitt(frame);
        frame.append(static_cast<char>(crc >> 8));
        frame.append(static_cast<char>(crc & 0xFF));
    }

    return frame;
}
