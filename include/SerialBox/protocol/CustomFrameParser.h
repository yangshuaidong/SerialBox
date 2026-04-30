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
