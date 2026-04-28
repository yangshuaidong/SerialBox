/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/SerialPortManager.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <memory>
#include "SerialPort.h"

/**
 * SerialPortManager — 多端口管理 + 热插拔检测
 */
class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    struct HardwareSignals
    {
        bool dtr = false;
        bool rts = false;
        bool cts = false;
        bool dsr = false;
    };

    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    // ── 端口操作 ──
    bool connectPort(const ISerialTransport::Config &config);
    void disconnectPort();
    bool isConnected() const;
    QString currentPortName() const;
    bool isSimulationMode() const { return m_simulationMode; }
    HardwareSignals hardwareSignals();
    void setOutputSignals(bool dtr, bool rts);

    // ── 热插拔 ──
    void enableHotplug(bool enable);
    bool hotplugEnabled() const { return m_hotplugEnabled; }

    // ── 收发 ──
    qint64 sendData(const QByteArray &data);

    // ── 统计 ──
    struct Stats
    {
        quint64 rxBytes = 0;
        quint64 txBytes = 0;
        qint64 connectedMs = 0;
    };
    Stats stats() const;

public slots:
    void refreshPorts();
    void setAutoReconnect(bool enable, int maxRetries = 5, int intervalMs = 3000);

signals:
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);
    void portsChanged(const QStringList &ports);
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void reconnecting(int attempt, int maxRetries);
    void reconnected();
    void statsUpdated(const Stats &stats);

private slots:
    void pollPorts();

private:
    std::unique_ptr<SerialPort> m_port;
    QTimer m_hotplugTimer;
    QTimer m_statsTimer;
    QStringList m_knownPorts;
    bool m_hotplugEnabled = true;
    Stats m_stats;
    QDateTime m_connectTime;
    bool m_simulationMode = false;
    QString m_simulationPortName;
    HardwareSignals m_hwSignals;
};
