/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/daemon/SerialBridge.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/daemon/SerialBridge.h"
#include "SerialBox/core/SerialPortManager.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QSerialPortInfo>

#ifdef ENABLE_WEBSOCKETS
#include <QWebSocketServer>
#include <QWebSocket>
#endif

SerialBridge::SerialBridge(QObject *parent)
    : QObject(parent)
{
}

SerialBridge::~SerialBridge()
{
    stop();
}

void SerialBridge::setSerialManager(SerialPortManager *mgr)
{
    m_serialManager = mgr;
    registerRpcMethods();
}

bool SerialBridge::start(quint16 port)
{
    m_port = port;
#ifdef ENABLE_WEBSOCKETS
    m_server = new QWebSocketServer("SerialBox Bridge",
                                     QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection, this, &SerialBridge::onNewConnection);

    // 串口事件转发给所有 WebSocket 客户端
    if (m_serialManager) {
        connect(m_serialManager, &SerialPortManager::dataReceived, this,
                [this](const QByteArray &data) {
                    broadcastEvent("serial.data", {
                        {"hex", QString(data.toHex(' ').toUpper())},
                        {"size", data.size()}
                    });
                });
        connect(m_serialManager, &SerialPortManager::connected, this,
                [this]() { broadcastEvent("serial.connected", {}); });
        connect(m_serialManager, &SerialPortManager::disconnected, this,
                [this]() { broadcastEvent("serial.disconnected", {}); });
        connect(m_serialManager, &SerialPortManager::errorOccurred, this,
                [this](const QString &err) {
                    broadcastEvent("serial.error", {{"message", err}});
                });
    }

    return true;
#else
    Q_UNUSED(port)
    qWarning("SerialBridge: WebSockets not available (build without Qt6WebSockets)");
    return false;
#endif
}

void SerialBridge::stop()
{
#ifdef ENABLE_WEBSOCKETS
    for (auto *client : m_clients) {
        client->close();
        client->deleteLater();
    }
    m_clients.clear();

    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
#endif
}

bool SerialBridge::isRunning() const
{
#ifdef ENABLE_WEBSOCKETS
    return m_server && m_server->isListening();
#else
    return false;
#endif
}

quint16 SerialBridge::port() const { return m_port; }

void SerialBridge::registerRpcMethods()
{
    // serial.list — 列出可用串口
    m_rpcServer.registerMethod("serial.list", [this](const QJsonObject &) {
        QJsonArray arr;
        for (const auto &info : QSerialPortInfo::availablePorts()) {
            arr.append(info.portName());
        }
        return QJsonValue(arr);
    });

    // serial.connect — 连接串口
    m_rpcServer.registerMethod("serial.connect", [this](const QJsonObject &params) {
        if (!m_serialManager) return QJsonValue(false);

        ISerialTransport::Config cfg;
        cfg.portName = params.value("port").toString();
        cfg.baudRate = params.value("baudRate", 115200).toInt();
        cfg.dataBits = params.value("dataBits", 8).toInt();
        cfg.stopBits = params.value("stopBits", 1).toInt();
        cfg.parity = params.value("parity", "None").toString();
        cfg.flowControl = params.value("flowControl", "None").toString();

        bool ok = m_serialManager->connectPort(cfg);
        return QJsonValue(ok);
    });

    // serial.disconnect — 断开串口
    m_rpcServer.registerMethod("serial.disconnect", [this](const QJsonObject &) {
        if (m_serialManager) m_serialManager->disconnectPort();
        return QJsonValue(true);
    });

    // serial.send — 发送数据（hex 格式）
    m_rpcServer.registerMethod("serial.send", [this](const QJsonObject &params) {
        if (!m_serialManager || !m_serialManager->isConnected())
            return QJsonValue(-1);

        QString hex = params.value("hex").toString().remove(' ');
        QByteArray data = QByteArray::fromHex(hex.toLatin1());
        qint64 written = m_serialManager->sendData(data);
        return QJsonValue(static_cast<int>(written));
    });

    // serial.stats — 获取收发统计
    m_rpcServer.registerMethod("serial.stats", [this](const QJsonObject &) {
        if (!m_serialManager) return QJsonValue(QJsonObject());
        auto st = m_serialManager->stats();
        return QJsonValue(QJsonObject{
            {"rxBytes", static_cast<double>(st.rxBytes)},
            {"txBytes", static_cast<double>(st.txBytes)},
            {"connectedMs", static_cast<double>(st.connectedMs)},
            {"connected", m_serialManager->isConnected()}
        });
    });

    // serial.status — 连接状态
    m_rpcServer.registerMethod("serial.status", [this](const QJsonObject &) {
        if (!m_serialManager) return QJsonValue(QJsonObject{{"connected", false}});
        return QJsonValue(QJsonObject{
            {"connected", m_serialManager->isConnected()},
            {"port", m_serialManager->currentPortName()}
        });
    });
}

#ifdef ENABLE_WEBSOCKETS
void SerialBridge::onNewConnection()
{
    auto *socket = m_server->nextPendingConnection();
    QString clientId = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
    m_clients.append(socket);
    emit clientConnected(clientId);

    connect(socket, &QWebSocket::textMessageReceived, this,
            [this, socket](const QString &message) { onTextMessage(socket, message); });
    connect(socket, &QWebSocket::disconnected, this, &SerialBridge::onSocketDisconnected);
}

void SerialBridge::onTextMessage(QWebSocket *sender, const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        QJsonObject err = JsonRpcServer::makeError(QJsonValue(), -32700, "Parse error");
        sender->sendTextMessage(QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    QJsonObject request = doc.object();
    QJsonObject response = m_rpcServer.processRequest(request);
    sender->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

void SerialBridge::onSocketDisconnected()
{
    auto *socket = qobject_cast<QWebSocket *>(sender());
    if (socket) {
        QString clientId = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
        m_clients.removeAll(socket);
        emit clientDisconnected(clientId);
        socket->deleteLater();
    }
}
#endif

void SerialBridge::broadcastEvent(const QString &event, const QJsonObject &data)
{
#ifdef ENABLE_WEBSOCKETS
    QJsonObject msg{
        {"jsonrpc", "2.0"},
        {"method", event},
        {"params", data}
    };
    QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    for (auto *client : m_clients) {
        client->sendTextMessage(QString::fromUtf8(payload));
    }
#else
    Q_UNUSED(event)
    Q_UNUSED(data)
#endif
}
