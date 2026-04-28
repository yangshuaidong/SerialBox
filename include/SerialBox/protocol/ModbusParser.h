/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/protocol/ModbusParser.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include "IProtocolParser.h"

/**
 * ModbusParser — Modbus RTU 帧解析
 *
 * 支持功能码 03/04/06/16，自动 CRC 校验
 */
class ModbusParser : public IProtocolParser
{
    Q_OBJECT

public:
    explicit ModbusParser(QObject *parent = nullptr);

    QString name() const override { return "Modbus RTU"; }
    QString description() const override { return "Modbus RTU 协议帧解析"; }

    bool match(const QByteArray &buffer) override;
    ParseResult parse(QByteArray &buffer) override;
    QByteArray build(const QVariantMap &fields) override;

    // ── Modbus 辅助 ──
    static quint16 crc16(const QByteArray &data);
    static QString funcCodeName(quint8 code);
};
