#pragma once
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <memory>

/**
 * ISerialTransport — 硬件抽象层 (HAL)
 *
 * 为 WebSerial / Wasm / 模拟器等预留替换接口
 */
class ISerialTransport : public QObject
{
    Q_OBJECT
public:
    explicit ISerialTransport(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ISerialTransport() = default;

    struct Config {
        QString portName;
        int baudRate = 115200;
        int dataBits = 8;
        int stopBits = 1;
        QString parity = "None";
        QString flowControl = "None";
        int readTimeoutMs = 1000;
    };

    virtual bool open(const Config &config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual QList<QSerialPortInfo> availablePorts() = 0;
    virtual QString lastError() const = 0;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void portClosed();
};
