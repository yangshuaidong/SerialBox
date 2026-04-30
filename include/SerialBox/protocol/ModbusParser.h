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

private:
    /**
     * 根据帧内容推算期望帧长度
     * @return 期望字节数，-1 表示无法确定
     */
    int expectedFrameLength(const QByteArray &buffer) const;

    /**
     * 判断是否为异常响应（FC | 0x80）
     */
    static bool isExceptionResponse(quint8 fc) { return fc & 0x80; }
};
