#include "SerialBox/daemon/JsonRpcServer.h"

JsonRpcServer::JsonRpcServer(QObject *parent)
    : QObject(parent)
{
}

void JsonRpcServer::registerMethod(const QString &method, Handler handler)
{
    m_handlers.insert(method, std::move(handler));
}

JsonRpcServer::ProcessOutput JsonRpcServer::processMessage(const QJsonObject &msg)
{
    ProcessOutput out;
    QString method = msg.value("method").toString();
    bool isNotification = !msg.contains("id");
    QJsonValue id = msg.value("id");

    if (method.isEmpty()) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, ErrorCodes::InvalidRequest,
                                     "Invalid Request: missing method");
        }
        return out;
    }

    auto it = m_handlers.constFind(method);
    if (it == m_handlers.constEnd()) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, ErrorCodes::MethodNotFound,
                                     QString("Method not found: %1").arg(method));
        }
        return out;
    }

    QJsonObject params = msg.value("params").toObject();
    try {
        QJsonValue result = it.value()(params);
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeResponse(id, result);
        } else {
            out.type = MessageType::Notification;
        }
    } catch (const std::exception &e) {
        if (!isNotification) {
            out.type = MessageType::Request;
            out.response = makeError(id, ErrorCodes::InternalError,
                                     QString("Internal error: %1").arg(e.what()));
        }
    }
    return out;
}

QJsonObject JsonRpcServer::processRequest(const QJsonObject &request)
{
    auto output = processMessage(request);
    if (output.type == MessageType::Request) {
        return output.response;
    }
    // Notification: 返回空对象（兼容旧调用方）
    return {};
}

QJsonObject JsonRpcServer::makeResponse(const QJsonValue &id, const QJsonValue &result)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

QJsonObject JsonRpcServer::makeError(const QJsonValue &id, int code, const QString &message)
{
    return {
        {"jsonrpc", "2.0"}, {"id", id},
        {"error", QJsonObject{{"code", code}, {"message", message}}}
    };
}
