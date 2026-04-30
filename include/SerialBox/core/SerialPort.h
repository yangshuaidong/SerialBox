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
