#pragma once
#include "IProtocolParser.h"

/**
 * ModbusParser — Modbus RTU 帧解析（完整功能码支持）
 *
 * 支持功能码: 0x01/0x02/0x03/0x04/0x05/0x06/0x0F/0x10
 * 含 CRC-16 校验，请求/响应帧智能区分
 */
class ModbusParser : public IProtocolParser
{
    Q_OBJECT

public:
    explicit ModbusParser(QObject *parent = nullptr);

    QString name() const override { return "Modbus"; }
    QString description() const override { return "Modbus RTU 帧解析（含 CRC 校验）"; }

    bool match(const QByteArray &buffer) override;
    ParseResult parse(QByteArray &buffer) override;
    QByteArray build(const QVariantMap &fields) override;

    static quint16 crc16(const QByteArray &data);
    static QString funcCodeName(quint8 code);

    /**
     * 通知解析器：刚发送了一帧请求
     * 用于区分同一 FC 的请求/响应帧（0x03/0x04）
     */
    void notifySent(quint8 fc);

private:
    int expectedFrameLength(const QByteArray &buffer) const;
    static bool isExceptionResponse(quint8 fc) { return fc & 0x80; }

    // 最后发送的请求 FC，用于请求/响应区分
    quint8 m_lastSentFc = 0;
};
