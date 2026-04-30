#include "SerialBox/ui/SerialDialog.h"
#include "SerialBox/app/ConfigManager.h"
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

SerialDialog::SerialDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("串口连接");
    setMinimumWidth(420);
    setModal(true);
    setupUi();
}

void SerialDialog::setConfigManager(ConfigManager *config)
{
    m_config = config;
}

void SerialDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(10);

    // 串口选择
    m_portCombo = new QComboBox(this);
    form->addRow("串口", m_portCombo);

    // 波特率
    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"115200", "57600", "38400", "19200", "9600",
                           "4800", "2400", "1200", "256000", "460800", "921600"});
    m_baudCombo->setObjectName("baudCombo");
    form->addRow("波特率", m_baudCombo);

    // 数据位
    m_dataBitsCombo = new QComboBox(this);
    m_dataBitsCombo->addItems({"8", "7", "6", "5"});
    form->addRow("数据位", m_dataBitsCombo);

    // 停止位
    m_stopBitsCombo = new QComboBox(this);
    m_stopBitsCombo->addItems({"1", "1.5", "2"});
    form->addRow("停止位", m_stopBitsCombo);

    // 校验位
    m_parityCombo = new QComboBox(this);
    m_parityCombo->addItems({"None", "Even", "Odd", "Mark", "Space"});
    form->addRow("校验位", m_parityCombo);

    // 流控
    m_flowCombo = new QComboBox(this);
    m_flowCombo->addItems({"None", "RTS/CTS", "XON/XOFF"});
    form->addRow("流控", m_flowCombo);

    mainLayout->addLayout(form);

    // 按钮区
    auto *btnLayout = new QHBoxLayout();
    auto *refreshBtn = new QPushButton("刷新列表 ↻", this);
    auto *advancedBtn = new QPushButton("高级设置...", this);
    auto *connectBtn = new QPushButton("🔗 连接", this);
    connectBtn->setDefault(true);

    connect(refreshBtn, &QPushButton::clicked, this, &SerialDialog::refreshPorts);
    connect(advancedBtn, &QPushButton::clicked, this, [this]()
            { emit advancedSettingsRequested(); });
    connect(connectBtn, &QPushButton::clicked, this, [this]()
            {
        if (!m_config) return;

        // 把 UI 选择写入 ConfigManager
        ConfigManager::SerialPreset preset;
        preset.portName    = m_portCombo->currentText().split(" — ").value(0).trimmed();
        preset.baudRate    = m_baudCombo->currentText().toInt();
        preset.dataBits    = m_dataBitsCombo->currentText().toInt();
        preset.stopBits    = m_stopBitsCombo->currentText().toInt();
        preset.parity      = m_parityCombo->currentText();
        preset.flowControl = m_flowCombo->currentText();
        preset.timeoutMs   = 1000;
        m_config->setSerialPreset(preset);

        emit connectRequested();
        accept(); });

    btnLayout->addWidget(refreshBtn);
    btnLayout->addWidget(advancedBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(connectBtn);
    mainLayout->addLayout(btnLayout);

    refreshPorts();
}

void SerialDialog::refreshPorts()
{
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts())
    {
        QString desc = info.description().isEmpty() ? info.manufacturer() : info.description();
        ports.append(QString("%1 — %2").arg(info.portName(), desc));
    }
    m_portCombo->clear();
    if (ports.isEmpty())
    {
        m_portCombo->addItem("(未检测到串口)");
    }
    else
    {
        m_portCombo->addItems(ports);
    }
}

void SerialDialog::updatePortList(const QStringList &ports)
{
    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    int idx = m_portCombo->findText(current);
    if (idx >= 0)
        m_portCombo->setCurrentIndex(idx);
}
