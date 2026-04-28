/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/protocol/CustomFrameParser.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include "IProtocolParser.h"

/**
 * CustomFrameParser — 自定义帧解析
 *
 * 支持用户定义：帧头、长度字段位置、校验方式
 */
class CustomFrameParser : public IProtocolParser
{
    Q_OBJECT

public:
    explicit CustomFrameParser(QObject *parent = nullptr);

    QString name() const override { return "自定义帧"; }
    QString description() const override { return "用户自定义帧格式解析"; }

    bool match(const QByteArray &buffer) override;
    ParseResult parse(QByteArray &buffer) override;
    QByteArray build(const QVariantMap &fields) override;

    // ── 帧格式配置 ──
    void setFrameHeader(const QByteArray &header);
    void setLengthOffset(int offset, int size);  // 长度字段位置与字节数
    void setCrcEnabled(bool enable);

private:
    QByteArray m_frameHeader = QByteArray::fromHex("AA55");
    int m_lengthOffset = 2;
    int m_lengthSize = 2;  // 2 字节长度
    bool m_crcEnabled = false;
};
