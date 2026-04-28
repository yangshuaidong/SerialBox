#include "SerialBox/core/SerialPort.h"

SerialPort::SerialPort(QObject *parent)
    : ISerialTransport(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialPort::onErrorOccurred);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SerialPort::attemptReconnect);
}

SerialPort::~SerialPort()
{
    close();
}

static QSerialPort::StopBits toStopBits(int val)
{
    switch (val)
    {
    case 2:
        return QSerialPort::TwoStop;
    default:
        return QSerialPort::OneStop;
    }
}

static QSerialPort::Parity toParity(const QString &val)
{
    if (val == "Even")
        return QSerialPort::EvenParity;
    if (val == "Odd")
        return QSerialPort::OddParity;
    if (val == "Mark")
        return QSerialPort::MarkParity;
    if (val == "Space")
        return QSerialPort::SpaceParity;
    return QSerialPort::NoParity;
}

static QSerialPort::FlowControl toFlowControl(const QString &val)
{
    if (val == "RTS/CTS")
        return QSerialPort::HardwareControl;
    if (val == "XON/XOFF")
        return QSerialPort::SoftwareControl;
    return QSerialPort::NoFlowControl;
}

bool SerialPort::open(const Config &config)
{
    m_config = config;
    m_port.setPortName(config.portName);
    m_port.setBaudRate(config.baudRate);
    m_port.setDataBits(static_cast<QSerialPort::DataBits>(config.dataBits));
    m_port.setStopBits(toStopBits(config.stopBits));
    m_port.setParity(toParity(config.parity));
    m_port.setFlowControl(toFlowControl(config.flowControl));

    if (!m_port.open(QIODevice::ReadWrite))
    {
        emit errorOccurred(m_port.errorString());
        return false;
    }
    m_retryCount = 0;
    return true;
}

void SerialPort::close()
{
    m_reconnectTimer.stop();
    m_autoReconnect = false; // 主动关闭则不再重连
    if (m_port.isOpen())
    {
        m_port.close();
        emit portClosed();
    }
}

bool SerialPort::isOpen() const { return m_port.isOpen(); }

qint64 SerialPort::write(const QByteArray &data)
{
    if (!m_port.isOpen())
        return -1;
    return m_port.write(data);
}

QList<QSerialPortInfo> SerialPort::availablePorts()
{
    return QSerialPortInfo::availablePorts();
}

QString SerialPort::lastError() const
{
    return m_port.errorString();
}

QString SerialPort::currentPortName() const
{
    return m_port.portName();
}

SerialPort::LineSignals SerialPort::lineSignals()
{
    LineSignals out;
    if (!m_port.isOpen())
    {
        return out;
    }

    const auto pins = m_port.pinoutSignals();
    out.dtr = m_port.isDataTerminalReady();
    out.rts = m_port.isRequestToSend();
    out.cts = pins.testFlag(QSerialPort::ClearToSendSignal);
    out.dsr = pins.testFlag(QSerialPort::DataSetReadySignal);
    return out;
}

void SerialPort::setLineOutputs(bool dtr, bool rts)
{
    if (!m_port.isOpen())
    {
        return;
    }
    m_port.setDataTerminalReady(dtr);
    m_port.setRequestToSend(rts);
}

void SerialPort::enableAutoReconnect(bool enable, int maxRetries, int intervalMs)
{
    m_autoReconnect = enable;
    m_maxRetries = maxRetries;
    m_reconnectTimer.setInterval(intervalMs);
}

void SerialPort::onReadyRead()
{
    QByteArray data = m_port.readAll();
    if (!data.isEmpty())
    {
        emit dataReceived(data);
    }
}

void SerialPort::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    emit errorOccurred(m_port.errorString());

    if (error == QSerialPort::ResourceError && m_autoReconnect)
    {
        m_port.close();
        m_retryCount = 0;
        m_reconnectTimer.start();
        emit portClosed();
    }
}

void SerialPort::attemptReconnect()
{
    if (m_retryCount >= m_maxRetries)
    {
        m_reconnectTimer.stop();
        emit errorOccurred(QString("自动重连失败: 已达最大重试次数 (%1)").arg(m_maxRetries));
        return;
    }

    ++m_retryCount;
    emit reconnecting(m_retryCount, m_maxRetries);

    if (m_port.open(QIODevice::ReadWrite))
    {
        m_reconnectTimer.stop();
        m_retryCount = 0;
        emit reconnected();
    }
}
