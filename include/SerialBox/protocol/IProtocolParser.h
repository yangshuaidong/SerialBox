#pragma once
#include <QObject>
#include <QByteArray>
#include <QVariantMap>

/**
 * IProtocolParser — 通用协议解析器接口
 *
 * 标准化 match / parse / build 流程
 */
class IProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit IProtocolParser(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IProtocolParser() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;

    // ── JSON-RPC 元数据（独立于 fields，避免污染用户数据）──
    struct RpcMeta {
        bool isRpc = false;
        QString type;       // "request" | "notification" | "response" | "error"
        QString method;
        int errorCode = 0;
        QString errorMsg;
    };

    // ── 数据 → 结构化报文 ──
    struct ParseResult {
        bool matched = false;
        QString protocolName;
        QVariantMap fields;          // 结构化字段（纯用户数据）
        QString displayText;         // 人类可读文本
        QList<QByteArray> rawFrames; // 完整帧
        RpcMeta rpcMeta;             // JSON-RPC 专用元数据
    };

    /**
     * match: 缓冲区中是否包含完整帧
     * parse: 提取并解析帧内容
     * build: 从字段构建发送帧
     */
    virtual bool match(const QByteArray &buffer) = 0;
    virtual ParseResult parse(QByteArray &buffer) = 0;
    virtual QByteArray build(const QVariantMap &fields) = 0;
};
