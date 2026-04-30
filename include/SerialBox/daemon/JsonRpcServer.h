#pragma once
#include <QObject>
#include <QJsonObject>
#include <functional>

/**
 * 统一错误码常量
 */
namespace ErrorCodes {
    // JSON-RPC 2.0 标准
    constexpr int ParseError     = -32700;
    constexpr int InvalidRequest = -32600;
    constexpr int MethodNotFound = -32601;
    constexpr int InvalidParams  = -32602;
    constexpr int InternalError  = -32603;

    // SerialBox 扩展 (-32000 ~ -32099)
    constexpr int PortNotOpen     = -32001;
    constexpr int PortBusy        = -32002;
    constexpr int WriteFailed     = -32003;
    constexpr int InvalidHex      = -32004;
    constexpr int CrcMismatch     = -32005;
    constexpr int FrameTimeout    = -32006;
}

/**
 * JsonRpcServer — JSON-RPC 2.0 本地回环通信
 *
 * 注册方法表，处理远程调用。
 * 严格区分 Request（有 id）与 Notification（无 id）：
 *   - Request    → 必须返回响应
 *   - Notification → 不得回复
 */
class JsonRpcServer : public QObject
{
    Q_OBJECT

public:
    explicit JsonRpcServer(QObject *parent = nullptr);

    using Handler = std::function<QJsonValue(const QJsonObject &params)>;
    void registerMethod(const QString &method, Handler handler);

    // ── 消息类型 ──
    enum class MessageType { Request, Notification, Invalid };

    struct ProcessOutput {
        MessageType type = MessageType::Invalid;
        QJsonObject response;  // 仅当 type == Request 时有效
    };

    /**
     * processMessage: 严格遵循 JSON-RPC 2.0
     *  - 有 id 字段 → Request → 返回 response
     *  - 无 id 字段 → Notification → response 为空，调用方不应发送
     */
    ProcessOutput processMessage(const QJsonObject &msg);

    // 保留旧接口兼容
    QJsonObject processRequest(const QJsonObject &request);

    static QJsonObject makeResponse(const QJsonValue &id, const QJsonValue &result);
    static QJsonObject makeError(const QJsonValue &id, int code, const QString &message);

private:
    QMap<QString, Handler> m_handlers;
};
