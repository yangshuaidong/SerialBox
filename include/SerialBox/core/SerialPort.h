/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/SerialPort.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include "ISerialTransport.h"
#include <QTimer>

/**
 * SerialPort — QSerialPort 的封装
 *
 * 功能：
 * - 串口读写
 * - 自动重连（可配重试策略）
 * - 编码处理
 */
class SerialPort : public ISerialTransport
{
    Q_OBJECT

public:
    struct LineSignals
    {
        bool dtr = false;
        bool rts = false;
        bool cts = false;
        bool dsr = false;
    };

    explicit SerialPort(QObject *parent = nullptr);
    ~SerialPort() override;

    bool open(const Config &config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    QList<QSerialPortInfo> availablePorts() override;
    QString lastError() const override;
    QString currentPortName() const;
    LineSignals lineSignals();
    void setLineOutputs(bool dtr, bool rts);

    // ── 断线自动重连 ──
    void enableAutoReconnect(bool enable, int maxRetries = 5, int intervalMs = 3000);
    bool autoReconnectEnabled() const { return m_autoReconnect; }
    int retryCount() const { return m_retryCount; }

signals:
    void reconnecting(int attempt, int maxRetries);
    void reconnected();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void attemptReconnect();

private:
    QSerialPort m_port;
    Config m_config;
    bool m_autoReconnect = false;
    int m_maxRetries = 5;
    int m_retryCount = 0;
    QTimer m_reconnectTimer;
};
