/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/daemon/JsonRpcServer.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/daemon/JsonRpcServer.h"

JsonRpcServer::JsonRpcServer(QObject *parent)
    : QObject(parent)
{
}

void JsonRpcServer::registerMethod(const QString &method, Handler handler)
{
    m_handlers.insert(method, std::move(handler));
}

QJsonObject JsonRpcServer::processRequest(const QJsonObject &request)
{
    QJsonValue id = request.value("id");
    QString method = request.value("method").toString();
    QJsonObject params = request.value("params").toObject();

    if (method.isEmpty()) {
        return makeError(id, -32600, "Invalid Request: missing method");
    }

    auto it = m_handlers.constFind(method);
    if (it == m_handlers.constEnd()) {
        return makeError(id, -32601, QString("Method not found: %1").arg(method));
    }

    try {
        QJsonValue result = it.value()(params);
        return makeResponse(id, result);
    } catch (const std::exception &e) {
        return makeError(id, -32000, QString("Internal error: %1").arg(e.what()));
    }
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
