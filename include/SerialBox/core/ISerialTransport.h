/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/ISerialTransport.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
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
