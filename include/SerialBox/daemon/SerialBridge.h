/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/daemon/SerialBridge.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QList>
#include "JsonRpcServer.h"

#ifdef ENABLE_WEBSOCKETS
class QWebSocketServer;
class QWebSocket;
#endif

class SerialPortManager;

/**
 * SerialBridge — WebSocket 中继守护进程
 *
 * serial-bridge: WebSocket 中继 + JSON-RPC 2.0
 * 支持多端口并发与事件订阅推送
 *
 * 需要 Qt6WebSockets，若未安装则此功能静默降级
 */
class SerialBridge : public QObject
{
    Q_OBJECT

public:
    explicit SerialBridge(QObject *parent = nullptr);
    ~SerialBridge() override;

    void setSerialManager(SerialPortManager *mgr);

    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    quint16 port() const;

signals:
    void clientConnected(const QString &clientId);
    void clientDisconnected(const QString &clientId);

private:
    void registerRpcMethods();
    void broadcastEvent(const QString &event, const QJsonObject &data);

#ifdef ENABLE_WEBSOCKETS
    void onNewConnection();
    void onTextMessage(QWebSocket *sender, const QString &message);
    void onSocketDisconnected();
#endif

    JsonRpcServer m_rpcServer;
    SerialPortManager *m_serialManager = nullptr;

#ifdef ENABLE_WEBSOCKETS
    QWebSocketServer *m_server = nullptr;
    QList<QWebSocket *> m_clients;
#endif
    quint16 m_port = 9876;
};
