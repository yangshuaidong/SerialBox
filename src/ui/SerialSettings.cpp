#include "SerialBox/ui/SerialSettings.h"
#include "SerialBox/app/ConfigManager.h"
#include <QSerialPortInfo>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

SerialSettings::SerialSettings(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("串口设置");
    setMinimumSize(640, 480);
    setupUi();
}

void SerialSettings::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(new QWidget(), "连接");
    m_tabs->addTab(new QWidget(), "参数");
    m_tabs->addTab(new QWidget(), "高级");
    m_tabs->addTab(new QWidget(), "预设");
    mainLayout->addWidget(m_tabs);

    // ── 连接 Tab ──
    {
        auto *tab = m_tabs->widget(0);
        auto *layout = new QVBoxLayout(tab);

        auto *title = new QLabel("可用端口", tab);
        title->setStyleSheet("font-weight: 600;");
        layout->addWidget(title);

        m_portList = new QListWidget(tab);
        m_portList->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(m_portList, 1);

        auto *refreshBtn = new QPushButton("刷新端口", tab);
        connect(refreshBtn, &QPushButton::clicked, this, [this]()
                {
            QStringList ports;
            for (const auto &info : QSerialPortInfo::availablePorts()) {
                const QString desc = info.description().isEmpty() ? info.manufacturer() : info.description();
                ports.append(desc.isEmpty() ? info.portName() : QString("%1 - %2").arg(info.portName(), desc));
            }
            updatePortList(ports); });
        layout->addWidget(refreshBtn);
        layout->addStretch();
    }

    // ── 参数 Tab ──
    {
        auto *tab = m_tabs->widget(1);
        auto *layout = new QFormLayout(tab);
        layout->setLabelAlignment(Qt::AlignRight);

        m_baudCombo = new QComboBox(tab);
        m_dataBitsCombo = new QComboBox(tab);
        m_stopBitsCombo = new QComboBox(tab);
        m_parityCombo = new QComboBox(tab);
        m_flowCombo = new QComboBox(tab);
        m_timeoutSpin = new QSpinBox(tab);
        m_timeoutSpin->setRange(10, 60000);
        m_timeoutSpin->setSuffix(" ms");

        layout->addRow("波特率", m_baudCombo);
        layout->addRow("数据位", m_dataBitsCombo);
        layout->addRow("停止位", m_stopBitsCombo);
        layout->addRow("校验位", m_parityCombo);
        layout->addRow("流控", m_flowCombo);
        layout->addRow("读写超时", m_timeoutSpin);
    }

    // ── 高级 Tab ──
    {
        auto *tab = m_tabs->widget(2);
        auto *layout = new QFormLayout(tab);
        layout->setLabelAlignment(Qt::AlignRight);

        m_autoReconnectCheck = new QCheckBox("启用断线自动重连", tab);
        m_retrySpin = new QSpinBox(tab);
        m_retrySpin->setRange(1, 99);
        m_intervalSpin = new QSpinBox(tab);
        m_intervalSpin->setRange(200, 60000);
        m_intervalSpin->setSuffix(" ms");

        layout->addRow(m_autoReconnectCheck);
        layout->addRow("最大重试次数", m_retrySpin);
        layout->addRow("重连间隔", m_intervalSpin);
    }

    // ── 预设 Tab ──
    {
        auto *tab = m_tabs->widget(3);
        auto *layout = new QVBoxLayout(tab);

        auto *row = new QHBoxLayout();
        m_presetCombo = new QComboBox(tab);
        m_presetNameEdit = new QLineEdit(tab);
        m_presetNameEdit->setPlaceholderText("输入预设名称");
        row->addWidget(new QLabel("已有预设", tab));
        row->addWidget(m_presetCombo, 1);
        layout->addLayout(row);

        layout->addWidget(m_presetNameEdit);

        auto *btnRow = new QHBoxLayout();
        auto *loadPresetBtn = new QPushButton("加载预设", tab);
        auto *savePresetBtn = new QPushButton("保存预设", tab);
        auto *deletePresetBtn = new QPushButton("删除预设", tab);
        btnRow->addWidget(loadPresetBtn);
        btnRow->addWidget(savePresetBtn);
        btnRow->addWidget(deletePresetBtn);
        btnRow->addStretch();
        layout->addLayout(btnRow);

        connect(loadPresetBtn, &QPushButton::clicked, this, [this]()
                {
            if (!m_configRef || !m_presetCombo) return;
            const QString name = m_presetCombo->currentText();
            if (name.isEmpty()) return;
            const auto presets = m_configRef->savedPresets();
            const auto names = m_configRef->presetNames();
            for (int i = 0; i < names.size() && i < presets.size(); ++i) {
                if (names[i] == name) {
                    const auto p = presets.value(i);
                    if (m_portList) {
                        for (int row = 0; row < m_portList->count(); ++row) {
                            auto *item = m_portList->item(row);
                            if (item && item->text().startsWith(p.portName)) {
                                m_portList->setCurrentRow(row);
                                break;
                            }
                        }
                    }
                    m_baudCombo->setCurrentText(QString::number(p.baudRate));
                    m_dataBitsCombo->setCurrentText(QString::number(p.dataBits));
                    m_stopBitsCombo->setCurrentText(QString::number(p.stopBits));
                    m_parityCombo->setCurrentText(p.parity);
                    m_flowCombo->setCurrentText(p.flowControl);
                    m_timeoutSpin->setValue(p.timeoutMs);
                    break;
                }
            } });

        connect(savePresetBtn, &QPushButton::clicked, this, [this]()
                {
            if (!m_configRef) return;
            const QString name = m_presetNameEdit ? m_presetNameEdit->text().trimmed() : QString();
            if (name.isEmpty()) {
                QMessageBox::information(this, "保存预设", "请输入预设名称。");
                return;
            }
            ConfigManager::SerialPreset p;
            p.portName = m_portList && m_portList->currentItem() ? m_portList->currentItem()->text().split(" - ").value(0).trimmed() : QString();
            p.baudRate = m_baudCombo->currentText().toInt();
            p.dataBits = m_dataBitsCombo->currentText().toInt();
            p.stopBits = m_stopBitsCombo->currentText() == "2" ? 2 : 1;
            p.parity = m_parityCombo->currentText();
            p.flowControl = m_flowCombo->currentText();
            p.timeoutMs = m_timeoutSpin->value();
            m_configRef->savePreset(name, p);
            m_configRef->save();
            refreshFromConfig(m_configRef);
            QMessageBox::information(this, "保存预设", "预设已保存。"); });

        connect(deletePresetBtn, &QPushButton::clicked, this, [this]()
                {
            if (!m_configRef || !m_presetCombo) return;
            const QString name = m_presetCombo->currentText();
            if (name.isEmpty()) return;
            m_configRef->deletePreset(name);
            m_configRef->save();
            refreshFromConfig(m_configRef); });

        layout->addStretch();
    }

    // ── 底部按钮 ──
    auto *btnLayout = new QHBoxLayout();
    auto *hotplugLabel = new QLabel("热插拔检测: 已启用", this);
    btnLayout->addWidget(hotplugLabel);
    btnLayout->addStretch();

    auto *resetBtn = new QPushButton("恢复默认", this);
    connect(resetBtn, &QPushButton::clicked, this, [this]()
            { populateDefaults(); });
    btnLayout->addWidget(resetBtn);

    auto *applyBtn = new QPushButton("应用", this);
    applyBtn->setDefault(true);
    connect(applyBtn, &QPushButton::clicked, this, [this]()
            {
        if (m_configRef) {
            ConfigManager::SerialPreset p;
            p.portName = m_portList && m_portList->currentItem() ? m_portList->currentItem()->text().split(" - ").value(0).trimmed() : QString();
            p.baudRate = m_baudCombo->currentText().toInt();
            p.dataBits = m_dataBitsCombo->currentText().toInt();
            p.stopBits = m_stopBitsCombo->currentText() == "2" ? 2 : 1;
            p.parity = m_parityCombo->currentText();
            p.flowControl = m_flowCombo->currentText();
            p.timeoutMs = m_timeoutSpin->value();
            m_configRef->setSerialPreset(p);
            m_configRef->save();
        }
        emit settingsApplied();
        accept(); });
    btnLayout->addWidget(applyBtn);

    mainLayout->addLayout(btnLayout);

    populateDefaults();
}

void SerialSettings::refreshFromConfig(ConfigManager *config)
{
    m_configRef = config;
    if (!config)
        return;

    const auto preset = config->serialPreset();
    m_baudCombo->setCurrentText(QString::number(preset.baudRate));
    m_dataBitsCombo->setCurrentText(QString::number(preset.dataBits));
    m_stopBitsCombo->setCurrentText(QString::number(preset.stopBits));
    m_parityCombo->setCurrentText(preset.parity);
    m_flowCombo->setCurrentText(preset.flowControl);
    m_timeoutSpin->setValue(preset.timeoutMs);

    if (!preset.portName.isEmpty() && m_portList)
    {
        for (int i = 0; i < m_portList->count(); ++i)
        {
            auto *item = m_portList->item(i);
            if (item && item->text().startsWith(preset.portName))
            {
                m_portList->setCurrentRow(i);
                break;
            }
        }
    }

    if (m_presetCombo)
    {
        m_presetCombo->clear();
        const auto names = config->presetNames();
        for (const QString &name : names)
        {
            m_presetCombo->addItem(name);
        }
    }
}

void SerialSettings::updatePortList(const QStringList &ports)
{
    if (!m_portList)
        return;

    QStringList targetPorts = ports;
    if (targetPorts.isEmpty())
    {
        for (const auto &info : QSerialPortInfo::availablePorts())
        {
            const QString desc = info.description().isEmpty() ? info.manufacturer() : info.description();
            targetPorts.append(desc.isEmpty() ? info.portName() : QString("%1 - %2").arg(info.portName(), desc));
        }
    }

    const QString current = m_portList->currentItem() ? m_portList->currentItem()->text() : QString();
    m_portList->clear();
    for (const QString &p : targetPorts)
    {
        m_portList->addItem(p);
    }
    if (m_portList->count() == 0)
    {
        m_portList->addItem("(未检测到串口)");
        m_portList->item(0)->setFlags(Qt::NoItemFlags);
        return;
    }
    for (int i = 0; i < m_portList->count(); ++i)
    {
        if (m_portList->item(i)->text() == current)
        {
            m_portList->setCurrentRow(i);
            return;
        }
    }
    m_portList->setCurrentRow(0);
}

void SerialSettings::populateDefaults()
{
    if (m_baudCombo && m_baudCombo->count() == 0)
    {
        m_baudCombo->addItems({"921600", "460800", "256000", "115200", "57600", "38400", "19200", "9600", "4800", "2400", "1200"});
    }
    if (m_dataBitsCombo && m_dataBitsCombo->count() == 0)
    {
        m_dataBitsCombo->addItems({"8", "7", "6", "5"});
    }
    if (m_stopBitsCombo && m_stopBitsCombo->count() == 0)
    {
        m_stopBitsCombo->addItems({"1", "2"});
    }
    if (m_parityCombo && m_parityCombo->count() == 0)
    {
        m_parityCombo->addItems({"None", "Even", "Odd", "Mark", "Space"});
    }
    if (m_flowCombo && m_flowCombo->count() == 0)
    {
        m_flowCombo->addItems({"None", "RTS/CTS", "XON/XOFF"});
    }
    if (m_timeoutSpin)
    {
        m_timeoutSpin->setValue(1000);
    }
    if (m_retrySpin)
    {
        m_retrySpin->setValue(5);
    }
    if (m_intervalSpin)
    {
        m_intervalSpin->setValue(3000);
    }
}
