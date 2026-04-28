/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/StatusBar.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/StatusBar.h"
#include "SerialBox/core/SerialPortManager.h"
#include <QLabel>
#include <QStyle>

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
    setupUi();
}

void StatusBar::setSerialManager(SerialPortManager *manager)
{
    m_serialManager = manager;
    connect(manager, &SerialPortManager::statsUpdated, this, [this](const auto &stats)
            {
        m_rxLabel->setText(QString("📥 RX: %1 B").arg(stats.rxBytes));
        m_txLabel->setText(QString("📤 TX: %1 B").arg(stats.txBytes));
        const quint64 rxSpeed = stats.rxBytes >= m_lastRxBytes ? (stats.rxBytes - m_lastRxBytes) : 0;
        const quint64 txSpeed = stats.txBytes >= m_lastTxBytes ? (stats.txBytes - m_lastTxBytes) : 0;
        m_lastRxBytes = stats.rxBytes;
        m_lastTxBytes = stats.txBytes;
        m_rxSpeedLabel->setText(QString("RX速率: %1 B/s").arg(rxSpeed));
        m_txSpeedLabel->setText(QString("TX速率: %1 B/s").arg(txSpeed));

        int secs = stats.connectedMs / 1000;
        int mins = secs / 60;
        secs %= 60;
        m_uptimeLabel->setText(QString("⏱ 连续 %1m %2s").arg(mins).arg(secs)); });
    connect(manager, &SerialPortManager::connected, this, [this]()
            {
        const QString portName = m_serialManager ? m_serialManager->currentPortName() : QString();
        m_portLabel->setText(portName.isEmpty() ? "端口: -" : QString("端口: %1").arg(portName));
        m_lastRxBytes = 0;
        m_lastTxBytes = 0;
        setStatusMessage("已连接", true); });
    connect(manager, &SerialPortManager::disconnected, this, [this]()
            {
        m_portLabel->setText("端口: -");
        m_rxSpeedLabel->setText("RX速率: 0 B/s");
        m_txSpeedLabel->setText("TX速率: 0 B/s");
        setStatusMessage("已断开", false); });
}

void StatusBar::setupUi()
{
    auto makeLabel = [this](const QString &text)
    {
        auto *l = new QLabel(text, this);
        return l;
    };

    m_portLabel = makeLabel("端口: -");
    m_portLabel->setObjectName("statusPort");
    m_statusLabel = makeLabel("未连接");
    m_statusLabel->setObjectName("statusConnection");
    m_statusLabel->setProperty("state", "disconnected");
    m_rxLabel = makeLabel("📥 RX: 0 B");
    m_rxLabel->setObjectName("statusRx");
    m_txLabel = makeLabel("📤 TX: 0 B");
    m_txLabel->setObjectName("statusTx");
    m_rxSpeedLabel = makeLabel("RX速率: 0 B/s");
    m_rxSpeedLabel->setObjectName("statusRxSpeed");
    m_txSpeedLabel = makeLabel("TX速率: 0 B/s");
    m_txSpeedLabel->setObjectName("statusTxSpeed");
    m_uptimeLabel = makeLabel("⏱ —");
    m_uptimeLabel->setObjectName("statusUptime");
    m_encodingLabel = makeLabel("编码: UTF-8");
    m_encodingLabel->setObjectName("statusEncoding");

    addWidget(m_portLabel);
    addWidget(m_statusLabel);

    auto *sep1 = new QLabel("│", this);
    sep1->setObjectName("statusSep");
    addWidget(sep1);

    addWidget(m_rxLabel);
    addWidget(m_txLabel);
    addWidget(m_rxSpeedLabel);
    addWidget(m_txSpeedLabel);

    auto *sep2 = new QLabel("│", this);
    sep2->setObjectName("statusSep");
    addWidget(sep2);

    addWidget(m_uptimeLabel);

    // 右侧
    addPermanentWidget(m_encodingLabel);
}

void StatusBar::setStatusMessage(const QString &msg, bool connected)
{
    m_statusLabel->setProperty("state", connected ? "connected" : "disconnected");
    m_statusLabel->setText(QString("● %1").arg(msg));
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->update();
}
