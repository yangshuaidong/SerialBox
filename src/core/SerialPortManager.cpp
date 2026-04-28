/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/core/SerialPortManager.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/core/SerialPortManager.h"
#include <QSerialPortInfo>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent), m_port(std::make_unique<SerialPort>(this))
{
    // 热插拔轮询 (每 2 秒)
    connect(&m_hotplugTimer, &QTimer::timeout, this, &SerialPortManager::pollPorts);
    m_hotplugTimer.start(2000);

    // 统计定时器 (每秒)
    connect(&m_statsTimer, &QTimer::timeout, this, [this]()
            {
        if (isConnected()) {
            m_stats.connectedMs = m_connectTime.msecsTo(QDateTime::currentDateTime());
            emit statsUpdated(m_stats);
        } });
    m_statsTimer.start(1000);

    // 串口信号转发
    connect(m_port.get(), &SerialPort::dataReceived, this, [this](const QByteArray &data)
            {
        m_stats.rxBytes += data.size();
        emit dataReceived(data); });
    connect(m_port.get(), &SerialPort::errorOccurred, this, &SerialPortManager::errorOccurred);
    connect(m_port.get(), &SerialPort::portClosed, this, &SerialPortManager::disconnected);
    connect(m_port.get(), &SerialPort::reconnecting, this, &SerialPortManager::reconnecting);
    connect(m_port.get(), &SerialPort::reconnected, this, &SerialPortManager::reconnected);

    // 初始扫描
    refreshPorts();
}

SerialPortManager::~SerialPortManager() { disconnectPort(); }

bool SerialPortManager::connectPort(const ISerialTransport::Config &config)
{
    if (m_port->isOpen())
        disconnectPort();

    if (config.portName.startsWith("SIM:", Qt::CaseInsensitive))
    {
        m_simulationMode = true;
        m_simulationPortName = config.portName;
        m_connectTime = QDateTime::currentDateTime();
        m_stats = Stats{};
        emit connected();
        return true;
    }

    m_simulationMode = false;
    m_simulationPortName.clear();

    if (m_port->open(config))
    {
        m_connectTime = QDateTime::currentDateTime();
        m_stats = Stats{};
        emit connected();
        return true;
    }
    return false;
}

void SerialPortManager::disconnectPort()
{
    if (m_simulationMode)
    {
        m_simulationMode = false;
        m_simulationPortName.clear();
        emit disconnected();
        return;
    }
    m_port->close();
}

bool SerialPortManager::isConnected() const
{
    return m_simulationMode || m_port->isOpen();
}

QString SerialPortManager::currentPortName() const
{
    if (m_simulationMode)
    {
        return m_simulationPortName;
    }
    if (!m_port || !m_port->isOpen())
    {
        return {};
    }
    return m_port->currentPortName();
}

qint64 SerialPortManager::sendData(const QByteArray &data)
{
    if (m_simulationMode)
    {
        if (data.isEmpty())
        {
            return 0;
        }
        m_stats.txBytes += static_cast<quint64>(data.size());
        emit dataSent(data);

        QByteArray response;
        if (m_simulationPortName.compare("SIM:Echo", Qt::CaseInsensitive) == 0)
        {
            response = data;
        }
        else if (m_simulationPortName.compare("SIM:AT", Qt::CaseInsensitive) == 0)
        {
            const QString cmd = QString::fromUtf8(data).trimmed().toUpper();
            if (cmd.startsWith("AT"))
            {
                response = "OK\\r\\n";
            }
            else
            {
                response = "ERROR\\r\\n";
            }
        }
        else
        {
            response = QByteArray::fromHex("01 03 02 00 64 B9 AF");
        }

        QTimer::singleShot(20, this, [this, response]()
                           {
            if (!m_simulationMode || response.isEmpty())
            {
                return;
            }
            m_stats.rxBytes += static_cast<quint64>(response.size());
            emit dataReceived(response); });
        return data.size();
    }

    qint64 written = m_port->write(data);
    if (written > 0)
    {
        m_stats.txBytes += written;
        emit dataSent(data);
    }
    return written;
}

SerialPortManager::Stats SerialPortManager::stats() const { return m_stats; }

void SerialPortManager::enableHotplug(bool enable)
{
    m_hotplugEnabled = enable;
    enable ? m_hotplugTimer.start() : m_hotplugTimer.stop();
}

void SerialPortManager::setAutoReconnect(bool enable, int maxRetries, int intervalMs)
{
    m_port->enableAutoReconnect(enable, maxRetries, intervalMs);
}

void SerialPortManager::refreshPorts()
{
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts())
    {
        ports.append(info.portName());
    }
    ports.append("SIM:Echo");
    ports.append("SIM:AT");
    ports.append("SIM:Modbus");
    std::sort(ports.begin(), ports.end());

    if (ports != m_knownPorts)
    {
        m_knownPorts = ports;
        emit portsChanged(ports);
    }
}

void SerialPortManager::pollPorts()
{
    if (!m_hotplugEnabled)
        return;
    refreshPorts();
}

SerialPortManager::HardwareSignals SerialPortManager::hardwareSignals()
{
    if (m_simulationMode)
    {
        return m_hwSignals;
    }

    if (!m_port || !m_port->isOpen())
    {
        return {};
    }

    const auto ls = m_port->lineSignals();
    HardwareSignals out;
    out.dtr = ls.dtr;
    out.rts = ls.rts;
    out.cts = ls.cts;
    out.dsr = ls.dsr;
    return out;
}

void SerialPortManager::setOutputSignals(bool dtr, bool rts)
{
    if (m_simulationMode)
    {
        m_hwSignals.dtr = dtr;
        m_hwSignals.rts = rts;
        m_hwSignals.cts = dtr;
        m_hwSignals.dsr = rts;
        return;
    }
    if (!m_port || !m_port->isOpen())
    {
        return;
    }
    m_port->setLineOutputs(dtr, rts);
}
