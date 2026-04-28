/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/daemon/JsonRpcServer.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QJsonObject>
#include <functional>

/**
 * JsonRpcServer — JSON-RPC 2.0 本地回环通信
 *
 * 注册方法表，处理远程调用
 */
class JsonRpcServer : public QObject
{
    Q_OBJECT

public:
    explicit JsonRpcServer(QObject *parent = nullptr);

    using Handler = std::function<QJsonValue(const QJsonObject &params)>;
    void registerMethod(const QString &method, Handler handler);

    QJsonObject processRequest(const QJsonObject &request);

    static QJsonObject makeResponse(const QJsonValue &id, const QJsonValue &result);
    static QJsonObject makeError(const QJsonValue &id, int code, const QString &message);

private:
    QMap<QString, Handler> m_handlers;
};
