#include "SerialBox/plugin/AdvancedLabPlugin.h"
#include "SerialBox/core/SerialPortManager.h"
#include "SerialBox/app/ConfigManager.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QDataStream>
#include <QPainter>
#include <QPolygonF>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QCryptographicHash>
#include <QtEndian>
#include <utility>

class SparklineWidget final : public QWidget
{
public:
    explicit SparklineWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(64);
    }

    void setSeries(const QVector<double> &points)
    {
        m_points = points;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#0f172a"));
        p.setRenderHint(QPainter::Antialiasing, true);

        if (m_points.size() < 2)
        {
            p.setPen(QColor("#93c5fd"));
            p.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter, QString::fromUtf8("等待数据"));
            return;
        }

        double maxV = 0.0;
        for (double v : m_points)
        {
            if (v > maxV)
            {
                maxV = v;
            }
        }
        if (maxV <= 0.0)
        {
            maxV = 1.0;
        }

        const int n = m_points.size();
        QPolygonF polyline;
        for (int i = 0; i < n; ++i)
        {
            const double x = (static_cast<double>(i) / (n - 1)) * (width() - 10) + 5;
            const double y = height() - 6 - (m_points.at(i) / maxV) * (height() - 12);
            polyline.append(QPointF(x, y));
        }

        p.setPen(QPen(QColor("#38bdf8"), 2));
        p.drawPolyline(polyline);
    }

private:
    QVector<double> m_points;
};
namespace
{
    static quint16 crc16Modbus(const QByteArray &data)
    {
        quint16 crc = 0xFFFF;
        for (unsigned char byte : data)
        {
            crc ^= byte;
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 0x0001)
                {
                    crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

    static quint32 crc32(const QByteArray &data)
    {
        quint32 crc = 0xFFFFFFFFu;
        for (unsigned char byte : data)
        {
            crc ^= static_cast<quint32>(byte);
            for (int i = 0; i < 8; ++i)
            {
                const bool lsb = (crc & 1u) != 0u;
                crc >>= 1;
                if (lsb)
                {
                    crc ^= 0xEDB88320u;
                }
            }
        }
        return ~crc;
    }
} // namespace

AdvancedLabPlugin::AdvancedLabPlugin(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void AdvancedLabPlugin::setSerialManager(SerialPortManager *manager)
{
    if (m_rxConn)
    {
        QObject::disconnect(m_rxConn);
    }
    if (m_txConn)
    {
        QObject::disconnect(m_txConn);
    }
    if (m_statsConn)
    {
        QObject::disconnect(m_statsConn);
    }

    m_serialManager = manager;
    if (!m_serialManager)
    {
        return;
    }

    m_rxConn = connect(m_serialManager, &SerialPortManager::dataReceived,
                       this, &AdvancedLabPlugin::onDataReceived);
    m_txConn = connect(m_serialManager, &SerialPortManager::dataSent,
                       this, [this](const QByteArray &data)
                       { processTriggerRules(data, false); });

    m_statsConn = connect(m_serialManager, &SerialPortManager::statsUpdated,
                          this, [this](const SerialPortManager::Stats &st)
                          {
                              if (!m_lastStatsTs.isValid())
                              {
                                  m_lastStatsTs = QDateTime::currentDateTime();
                                  m_prevRxBytes = st.rxBytes;
                                  m_prevTxBytes = st.txBytes;
                                  return;
                              }

                              const QDateTime now = QDateTime::currentDateTime();
                              double dt = m_lastStatsTs.msecsTo(now) / 1000.0;
                              if (dt <= 0.0)
                              {
                                  dt = 1.0;
                              }

                              const double rxRate = (static_cast<double>(st.rxBytes - m_prevRxBytes)) / dt;
                              const double txRate = (static_cast<double>(st.txBytes - m_prevTxBytes)) / dt;
                              m_prevRxBytes = st.rxBytes;
                              m_prevTxBytes = st.txBytes;
                              m_lastStatsTs = now;

                              if (m_rxRateLabel)
                              {
                                  m_rxRateLabel->setText(QString("RX 吞吐: %1 B/s").arg(QString::number(rxRate, 'f', 1)));
                              }
                              if (m_txRateLabel)
                              {
                                  m_txRateLabel->setText(QString("TX 吞吐: %1 B/s").arg(QString::number(txRate, 'f', 1)));
                              }

                              static QVector<double> rxSeries;
                              static QVector<double> txSeries;
                              rxSeries.append(rxRate);
                              txSeries.append(txRate);
                              while (rxSeries.size() > 60)
                              {
                                  rxSeries.removeFirst();
                              }
                              while (txSeries.size() > 60)
                              {
                                  txSeries.removeFirst();
                              }
                              if (m_rxSpark)
                              {
                                  m_rxSpark->setSeries(rxSeries);
                              }
                              if (m_txSpark)
                              {
                                  m_txSpark->setSeries(txSeries);
                              }
                              updateTrafficUi(); });

    refreshMonitorPortCombos();
}

void AdvancedLabPlugin::setupUi()
{
    setWindowTitle(QString::fromUtf8("高级调试中心"));
    resize(980, 700);
    setWindowFlags(Qt::Window);

    auto *layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildMultiSerialTab(), QString::fromUtf8("多串口监控"));
    m_tabs->addTab(buildReverseAssistTab(), QString::fromUtf8("协议逆向辅助"));
    m_tabs->addTab(buildTrafficTab(), QString::fromUtf8("流量统计"));
    m_tabs->addTab(buildTriggerTab(), QString::fromUtf8("脚本触发器"));
    m_tabs->addTab(buildConvertTab(), QString::fromUtf8("数据转换管道"));
    m_tabs->addTab(buildCrcTab(), QString::fromUtf8("CRC校验"));
    m_tabs->addTab(buildPluginTab(), QString::fromUtf8("内置插件"));
    m_tabs->addTab(buildTemplateTab(), QString::fromUtf8("协议模板市场"));
    m_tabs->addTab(buildSignalTab(), QString::fromUtf8("硬件信号监控"));
    m_tabs->addTab(buildSimulationTab(), QString::fromUtf8("仿真模式"));
    m_tabs->addTab(buildFuzzTab(), QString::fromUtf8("协议Fuzzing"));
    layout->addWidget(m_tabs);
}

void AdvancedLabPlugin::setPluginContext(ConfigManager *config)
{
    m_config = config;
    if (m_builtinBerCheck && m_config)
    {
        m_builtinBerCheck->setChecked(m_config->pluginEnabled("builtin.ber", true));
    }
    if (m_builtinWaveCheck && m_config)
    {
        m_builtinWaveCheck->setChecked(m_config->pluginEnabled("builtin.waveform", true));
    }
    if (m_builtinReportCheck && m_config)
    {
        m_builtinReportCheck->setChecked(m_config->pluginEnabled("builtin.report", true));
    }
}

void AdvancedLabPlugin::showBuiltinPluginTab()
{
    if (!m_tabs)
    {
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        if (m_tabs->tabText(i).contains(QString::fromUtf8("内置插件")))
        {
            m_tabs->setCurrentIndex(i);
            break;
        }
    }
}

QWidget *AdvancedLabPlugin::buildMultiSerialTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    auto *ctrl = new QHBoxLayout();
    m_leftPortCombo = new QComboBox(w);
    m_rightPortCombo = new QComboBox(w);
    m_leftPortCombo->setEditable(true);
    m_rightPortCombo->setEditable(true);
    auto *refreshBtn = new QPushButton(QString::fromUtf8("刷新端口"), w);
    auto *leftBtn = new QPushButton(QString::fromUtf8("连接左侧"), w);
    auto *rightBtn = new QPushButton(QString::fromUtf8("连接右侧"), w);
    ctrl->addWidget(new QLabel(QString::fromUtf8("左串口"), w));
    ctrl->addWidget(m_leftPortCombo);
    ctrl->addWidget(leftBtn);
    ctrl->addSpacing(24);
    ctrl->addWidget(new QLabel(QString::fromUtf8("右串口"), w));
    ctrl->addWidget(m_rightPortCombo);
    ctrl->addWidget(rightBtn);
    ctrl->addWidget(refreshBtn);
    layout->addLayout(ctrl);

    auto *split = new QHBoxLayout();
    m_leftMonitorLog = new QPlainTextEdit(w);
    m_rightMonitorLog = new QPlainTextEdit(w);
    m_leftMonitorLog->setReadOnly(true);
    m_rightMonitorLog->setReadOnly(true);
    split->addWidget(m_leftMonitorLog, 1);
    split->addWidget(m_rightMonitorLog, 1);
    layout->addLayout(split, 1);

    m_leftMonitorPort = new QSerialPort(this);
    m_rightMonitorPort = new QSerialPort(this);
    m_leftMonitorPort->setBaudRate(115200);
    m_rightMonitorPort->setBaudRate(115200);

    connect(refreshBtn, &QPushButton::clicked, this, [this]()
            { refreshMonitorPortCombos(); });

    connect(leftBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_leftMonitorPort || !m_leftPortCombo)
                {
                    return;
                }
                const QString port = m_leftPortCombo->currentText().trimmed();
                if (port.isEmpty())
                {
                    QMessageBox::warning(this, QString::fromUtf8("多串口监控"), QString::fromUtf8("请选择左侧串口"));
                    return;
                }
                if (m_leftMonitorPort->isOpen())
                {
                    m_leftMonitorPort->close();
                }
                m_leftMonitorPort->setPortName(port);
                if (!m_leftMonitorPort->open(QIODevice::ReadOnly))
                {
                    QMessageBox::warning(this, QString::fromUtf8("多串口监控"), QString::fromUtf8("左串口打开失败"));
                    return;
                }
                m_leftMonitorLog->appendPlainText(QString::fromUtf8("左侧已连接: ") + port); });

    connect(rightBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_rightMonitorPort || !m_rightPortCombo)
                {
                    return;
                }
                const QString port = m_rightPortCombo->currentText().trimmed();
                if (port.isEmpty())
                {
                    QMessageBox::warning(this, QString::fromUtf8("多串口监控"), QString::fromUtf8("请选择右侧串口"));
                    return;
                }
                if (m_rightMonitorPort->isOpen())
                {
                    m_rightMonitorPort->close();
                }
                m_rightMonitorPort->setPortName(port);
                if (!m_rightMonitorPort->open(QIODevice::ReadOnly))
                {
                    QMessageBox::warning(this, QString::fromUtf8("多串口监控"), QString::fromUtf8("右串口打开失败"));
                    return;
                }
                m_rightMonitorLog->appendPlainText(QString::fromUtf8("右侧已连接: ") + port); });

    connect(m_leftMonitorPort, &QSerialPort::readyRead, this, [this]()
            {
                const QByteArray d = m_leftMonitorPort->readAll();
                m_leftMonitorLog->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") + QString::fromUtf8(d.toHex(' ').toUpper())); });

    connect(m_rightMonitorPort, &QSerialPort::readyRead, this, [this]()
            {
                const QByteArray d = m_rightMonitorPort->readAll();
                m_rightMonitorLog->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") + QString::fromUtf8(d.toHex(' ').toUpper())); });

    refreshMonitorPortCombos();

    return w;
}

QWidget *AdvancedLabPlugin::buildReverseAssistTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    m_reverseAutoCheck = new QCheckBox(QString::fromUtf8("实时分析接收流"), w);
    m_reverseAutoCheck->setChecked(true);
    layout->addWidget(m_reverseAutoCheck);

    m_reverseInput = new QPlainTextEdit(w);
    m_reverseInput->setPlaceholderText(QString::fromUtf8("粘贴 HEX 数据流，例如: C0 01 02 DB DC C0"));
    layout->addWidget(m_reverseInput, 1);

    auto *btn = new QPushButton(QString::fromUtf8("分析协议边界"), w);
    layout->addWidget(btn);

    m_reverseHints = new QListWidget(w);
    layout->addWidget(m_reverseHints, 1);

    connect(btn, &QPushButton::clicked, this, [this]()
            {
                bool ok = false;
                const QByteArray data = parseHex(m_reverseInput->toPlainText(), &ok);
                if (!ok || data.isEmpty())
                {
                    QMessageBox::warning(this, QString::fromUtf8("协议逆向"), QString::fromUtf8("请输入合法 HEX 数据"));
                    return;
                }
                analyzePayload(data); });

    return w;
}

QWidget *AdvancedLabPlugin::buildTrafficTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    m_rxRateLabel = new QLabel(QString::fromUtf8("RX 吞吐: 0 B/s"), w);
    m_txRateLabel = new QLabel(QString::fromUtf8("TX 吞吐: 0 B/s"), w);
    m_errorFrameLabel = new QLabel(QString::fromUtf8("错误帧: 0"), w);
    m_retransLabel = new QLabel(QString::fromUtf8("重传率: 0.00%"), w);
    layout->addWidget(m_rxRateLabel);
    layout->addWidget(m_txRateLabel);
    layout->addWidget(m_errorFrameLabel);
    layout->addWidget(m_retransLabel);

    layout->addWidget(new QLabel(QString::fromUtf8("RX 曲线"), w));
    m_rxSpark = new SparklineWidget(w);
    layout->addWidget(m_rxSpark);

    layout->addWidget(new QLabel(QString::fromUtf8("TX 曲线"), w));
    m_txSpark = new SparklineWidget(w);
    layout->addWidget(m_txSpark);
    return w;
}

QWidget *AdvancedLabPlugin::buildTriggerTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    auto *form = new QFormLayout();
    m_triggerKeywordEdit = new QLineEdit(w);
    m_triggerResponseEdit = new QLineEdit(w);
    m_triggerResponseEdit->setPlaceholderText(QString::fromUtf8("命中后自动发送的响应文本"));
    form->addRow(QString::fromUtf8("关键字"), m_triggerKeywordEdit);
    form->addRow(QString::fromUtf8("自动响应"), m_triggerResponseEdit);
    layout->addLayout(form);

    m_triggerAlertCheck = new QCheckBox(QString::fromUtf8("命中时弹窗告警"), w);
    layout->addWidget(m_triggerAlertCheck);

    auto *addBtn = new QPushButton(QString::fromUtf8("添加触发规则"), w);
    layout->addWidget(addBtn);

    m_triggerList = new QListWidget(w);
    layout->addWidget(m_triggerList, 1);

    connect(addBtn, &QPushButton::clicked, this, [this]()
            {
                const QString key = m_triggerKeywordEdit->text().trimmed();
                const QString rsp = m_triggerResponseEdit->text();
                if (key.isEmpty())
                {
                    return;
                }
                TriggerRule rule;
                rule.keyword = key;
                rule.response = rsp;
                rule.enabled = true;
                m_triggers.append(rule);
                m_triggerList->addItem(QString::fromUtf8("[%1] 关键字=%2 响应=%3")
                                           .arg(rule.enabled ? QString::fromUtf8("启用") : QString::fromUtf8("禁用"),
                                                rule.keyword,
                                                rule.response));
                m_triggerKeywordEdit->clear(); });

    return w;
}

QWidget *AdvancedLabPlugin::buildConvertTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QFormLayout(w);

    m_convertHexInput = new QLineEdit(w);
    m_convertHexInput->setPlaceholderText(QString::fromUtf8("输入HEX或文本"));
    m_convertBase64Output = new QLineEdit(w);
    m_convertUtf16Output = new QLineEdit(w);
    m_convertCrc16Output = new QLineEdit(w);
    m_convertCrc32Output = new QLineEdit(w);

    m_convertBase64Output->setReadOnly(true);
    m_convertUtf16Output->setReadOnly(true);
    m_convertCrc16Output->setReadOnly(true);
    m_convertCrc32Output->setReadOnly(true);

    layout->addRow(QString::fromUtf8("输入"), m_convertHexInput);
    layout->addRow(QString::fromUtf8("Base64"), m_convertBase64Output);
    layout->addRow(QString::fromUtf8("UTF-16LE HEX"), m_convertUtf16Output);
    layout->addRow(QString::fromUtf8("CRC16"), m_convertCrc16Output);
    layout->addRow(QString::fromUtf8("CRC32"), m_convertCrc32Output);

    connect(m_convertHexInput, &QLineEdit::textChanged, this, [this](const QString &text)
            {
                QByteArray data;
                bool ok = false;
                data = parseHex(text, &ok);
                if (!ok)
                {
                    data = text.toUtf8();
                }
                m_convertBase64Output->setText(QString::fromLatin1(data.toBase64()));

                const QString utf16 = QString::fromUtf8(data);
                QByteArray utf16le;
                utf16le.resize(utf16.size() * 2);
                for (int i = 0; i < utf16.size(); ++i)
                {
                    const ushort u = utf16.at(i).unicode();
                    utf16le[i * 2] = static_cast<char>(u & 0xFF);
                    utf16le[i * 2 + 1] = static_cast<char>((u >> 8) & 0xFF);
                }
                m_convertUtf16Output->setText(QString::fromUtf8(utf16le.toHex(' ').toUpper()));
                m_convertCrc16Output->setText(QString("0x%1").arg(crc16Modbus(data), 4, 16, QChar('0')).toUpper());
                m_convertCrc32Output->setText(QString("0x%1").arg(crc32(data), 8, 16, QChar('0')).toUpper()); });

    return w;
}

QWidget *AdvancedLabPlugin::buildCrcTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    auto *form = new QFormLayout();
    m_crcAlgoCombo = new QComboBox(w);
    m_crcAlgoCombo->addItems({"CRC-16/MODBUS", "CRC-32"});
    m_crcEndianCombo = new QComboBox(w);
    m_crcEndianCombo->addItems({QString::fromUtf8("低字节在前(Little Endian)"), QString::fromUtf8("高字节在前(Big Endian)")});
    form->addRow(QString::fromUtf8("算法"), m_crcAlgoCombo);
    form->addRow(QString::fromUtf8("校验字节序"), m_crcEndianCombo);
    layout->addLayout(form);

    auto *calcTitle = new QLabel(QString::fromUtf8("输入命令计算 CRC"), w);
    calcTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(calcTitle);

    auto *calcRow = new QHBoxLayout();
    m_crcCalcInput = new QLineEdit(w);
    m_crcCalcInput->setPlaceholderText(QString::fromUtf8("输入命令（HEX: 01 03 00 00 00 01 或 文本）"));
    auto *calcBtn = new QPushButton(QString::fromUtf8("计算CRC"), w);
    calcRow->addWidget(m_crcCalcInput, 1);
    calcRow->addWidget(calcBtn);
    layout->addLayout(calcRow);

    m_crcCalcOutput = new QLineEdit(w);
    m_crcCalcOutput->setReadOnly(true);
    m_crcCalcOutput->setPlaceholderText(QString::fromUtf8("计算结果"));
    layout->addWidget(m_crcCalcOutput);

    auto *verifyTitle = new QLabel(QString::fromUtf8("输入命令进行 CRC 校验（命令末尾需含CRC）"), w);
    verifyTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(verifyTitle);

    auto *verifyRow = new QHBoxLayout();
    m_crcVerifyInput = new QLineEdit(w);
    m_crcVerifyInput->setPlaceholderText(QString::fromUtf8("例如: 01 03 00 00 00 01 84 0A"));
    auto *verifyBtn = new QPushButton(QString::fromUtf8("校验CRC"), w);
    verifyRow->addWidget(m_crcVerifyInput, 1);
    verifyRow->addWidget(verifyBtn);
    layout->addLayout(verifyRow);

    m_crcVerifyResult = new QLabel(QString::fromUtf8("校验结果: -"), w);
    layout->addWidget(m_crcVerifyResult);
    layout->addStretch();

    auto calculate = [this]()
    {
        if (!m_crcCalcInput || !m_crcCalcOutput || !m_crcAlgoCombo)
        {
            return;
        }
        const QString text = m_crcCalcInput->text().trimmed();
        if (text.isEmpty())
        {
            m_crcCalcOutput->clear();
            return;
        }

        bool ok = false;
        QByteArray data = parseHex(text, &ok);
        if (!ok)
        {
            data = text.toUtf8();
        }

        if (m_crcAlgoCombo->currentIndex() == 0)
        {
            const quint16 crc = crc16Modbus(data);
            const QString le = QString("%1 %2")
                                   .arg(crc & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 8) & 0xFF, 2, 16, QChar('0'))
                                   .toUpper();
            const QString be = QString("%1 %2")
                                   .arg((crc >> 8) & 0xFF, 2, 16, QChar('0'))
                                   .arg(crc & 0xFF, 2, 16, QChar('0'))
                                   .toUpper();
            m_crcCalcOutput->setText(QString("CRC16=0x%1 | LE:%2 | BE:%3")
                                         .arg(crc, 4, 16, QChar('0'))
                                         .arg(le)
                                         .arg(be)
                                         .toUpper());
        }
        else
        {
            const quint32 crc = crc32(data);
            const QString le = QString("%1 %2 %3 %4")
                                   .arg((crc) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 8) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 16) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 24) & 0xFF, 2, 16, QChar('0'))
                                   .toUpper();
            const QString be = QString("%1 %2 %3 %4")
                                   .arg((crc >> 24) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 16) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc >> 8) & 0xFF, 2, 16, QChar('0'))
                                   .arg((crc) & 0xFF, 2, 16, QChar('0'))
                                   .toUpper();
            m_crcCalcOutput->setText(QString("CRC32=0x%1 | LE:%2 | BE:%3")
                                         .arg(crc, 8, 16, QChar('0'))
                                         .arg(le)
                                         .arg(be)
                                         .toUpper());
        }
    };

    connect(calcBtn, &QPushButton::clicked, this, [calculate]()
            { calculate(); });
    connect(m_crcCalcInput, &QLineEdit::textChanged, this, [calculate](const QString &)
            { calculate(); });

    connect(verifyBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_crcVerifyInput || !m_crcVerifyResult || !m_crcAlgoCombo || !m_crcEndianCombo)
                {
                    return;
                }
                bool ok = false;
                QByteArray raw = parseHex(m_crcVerifyInput->text(), &ok);
                if (!ok || raw.isEmpty())
                {
                    m_crcVerifyResult->setText(QString::fromUtf8("校验结果: 输入格式错误（需HEX）"));
                    return;
                }

                bool pass = false;
                QString detail;

                if (m_crcAlgoCombo->currentIndex() == 0)
                {
                    if (raw.size() < 3)
                    {
                        m_crcVerifyResult->setText(QString::fromUtf8("校验结果: 数据过短"));
                        return;
                    }
                    const QByteArray payload = raw.left(raw.size() - 2);
                    const quint16 calc = crc16Modbus(payload);
                    const quint8 b0 = static_cast<quint8>(raw.at(raw.size() - 2));
                    const quint8 b1 = static_cast<quint8>(raw.at(raw.size() - 1));
                    const quint16 recv = (m_crcEndianCombo->currentIndex() == 0)
                                             ? static_cast<quint16>(b0 | (b1 << 8))
                                             : static_cast<quint16>((b0 << 8) | b1);
                    pass = (calc == recv);
                    detail = QString("recv=0x%1 calc=0x%2")
                                 .arg(recv, 4, 16, QChar('0'))
                                 .arg(calc, 4, 16, QChar('0'))
                                 .toUpper();
                }
                else
                {
                    if (raw.size() < 5)
                    {
                        m_crcVerifyResult->setText(QString::fromUtf8("校验结果: 数据过短"));
                        return;
                    }
                    const QByteArray payload = raw.left(raw.size() - 4);
                    const quint32 calc = crc32(payload);
                    const quint8 b0 = static_cast<quint8>(raw.at(raw.size() - 4));
                    const quint8 b1 = static_cast<quint8>(raw.at(raw.size() - 3));
                    const quint8 b2 = static_cast<quint8>(raw.at(raw.size() - 2));
                    const quint8 b3 = static_cast<quint8>(raw.at(raw.size() - 1));
                    quint32 recv = 0;
                    if (m_crcEndianCombo->currentIndex() == 0)
                    {
                        recv = static_cast<quint32>(b0) |
                               (static_cast<quint32>(b1) << 8) |
                               (static_cast<quint32>(b2) << 16) |
                               (static_cast<quint32>(b3) << 24);
                    }
                    else
                    {
                        recv = (static_cast<quint32>(b0) << 24) |
                               (static_cast<quint32>(b1) << 16) |
                               (static_cast<quint32>(b2) << 8) |
                               static_cast<quint32>(b3);
                    }
                    pass = (calc == recv);
                    detail = QString("recv=0x%1 calc=0x%2")
                                 .arg(recv, 8, 16, QChar('0'))
                                 .arg(calc, 8, 16, QChar('0'))
                                 .toUpper();
                }

                m_crcVerifyResult->setText(QString::fromUtf8("校验结果: %1 (%2)")
                                               .arg(pass ? QString::fromUtf8("通过") : QString::fromUtf8("失败"),
                                                    detail)); });

    return w;
}

QWidget *AdvancedLabPlugin::buildPluginTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    auto *builtinTitle = new QLabel(QString::fromUtf8("内置插件控制"), w);
    builtinTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(builtinTitle);

    auto *builtinRow = new QGridLayout();

    m_builtinBerCheck = new QCheckBox(QString::fromUtf8("误码率测试插件"), w);
    auto *berOpenBtn = new QPushButton(QString::fromUtf8("打开"), w);
    builtinRow->addWidget(m_builtinBerCheck, 0, 0);
    builtinRow->addWidget(berOpenBtn, 0, 1);

    m_builtinWaveCheck = new QCheckBox(QString::fromUtf8("实时波形插件"), w);
    auto *waveOpenBtn = new QPushButton(QString::fromUtf8("打开"), w);
    builtinRow->addWidget(m_builtinWaveCheck, 1, 0);
    builtinRow->addWidget(waveOpenBtn, 1, 1);

    m_builtinReportCheck = new QCheckBox(QString::fromUtf8("测试报告插件"), w);
    auto *reportOpenBtn = new QPushButton(QString::fromUtf8("打开"), w);
    builtinRow->addWidget(m_builtinReportCheck, 2, 0);
    builtinRow->addWidget(reportOpenBtn, 2, 1);

    layout->addLayout(builtinRow);

    connect(m_builtinBerCheck, &QCheckBox::toggled, this, [this](bool checked)
            { emit builtinPluginToggled("builtin.ber", checked); });
    connect(m_builtinWaveCheck, &QCheckBox::toggled, this, [this](bool checked)
            { emit builtinPluginToggled("builtin.waveform", checked); });
    connect(m_builtinReportCheck, &QCheckBox::toggled, this, [this](bool checked)
            { emit builtinPluginToggled("builtin.report", checked); });

    connect(berOpenBtn, &QPushButton::clicked, this, [this]()
            { emit builtinPluginOpenRequested("builtin.ber"); });
    connect(waveOpenBtn, &QPushButton::clicked, this, [this]()
            { emit builtinPluginOpenRequested("builtin.waveform"); });
    connect(reportOpenBtn, &QPushButton::clicked, this, [this]()
            { emit builtinPluginOpenRequested("builtin.report"); });
    auto *note = new QLabel(QString::fromUtf8("已移除动态插件安装与管理功能，仅支持内置插件。"), w);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return w;
}

QWidget *AdvancedLabPlugin::buildTemplateTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    m_templateList = new QListWidget(w);
    m_templateList->addItem(QString::fromUtf8("AT 指令模板: AT+GMR"));
    m_templateList->addItem(QString::fromUtf8("NMEA 模板: $GPRMC,....*hh"));
    m_templateList->addItem(QString::fromUtf8("CAN over Serial 模板: 18FF50E5 08 11 22 33 44 55 66 77 88"));
    m_templateList->addItem(QString::fromUtf8("Modbus 读保持寄存器: 01 03 00 00 00 01 84 0A"));
    layout->addWidget(m_templateList, 1);

    auto *btn = new QPushButton(QString::fromUtf8("应用到发送区"), w);
    layout->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this]()
            {
                if (!m_templateList || !m_templateList->currentItem())
                {
                    return;
                }
                const QString text = m_templateList->currentItem()->text();
                const int idx = text.indexOf(":");
                const QString cmd = (idx >= 0) ? text.mid(idx + 1).trimmed() : text;
                emit templateCommandRequested(cmd); });
    return w;
}

QWidget *AdvancedLabPlugin::buildSignalTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    m_dtrLabel = new QLabel("DTR: -", w);
    m_rtsLabel = new QLabel("RTS: -", w);
    m_ctsLabel = new QLabel("CTS: -", w);
    m_dsrLabel = new QLabel("DSR: -", w);
    layout->addWidget(m_dtrLabel);
    layout->addWidget(m_rtsLabel);
    layout->addWidget(m_ctsLabel);
    layout->addWidget(m_dsrLabel);

    m_dtrSetCheck = new QCheckBox("Set DTR", w);
    m_rtsSetCheck = new QCheckBox("Set RTS", w);
    layout->addWidget(m_dtrSetCheck);
    layout->addWidget(m_rtsSetCheck);

    connect(m_dtrSetCheck, &QCheckBox::toggled, this, [this](bool)
            {
                if (!m_serialManager)
                {
                    return;
                }
                m_serialManager->setOutputSignals(m_dtrSetCheck->isChecked(), m_rtsSetCheck->isChecked()); });

    connect(m_rtsSetCheck, &QCheckBox::toggled, this, [this](bool)
            {
                if (!m_serialManager)
                {
                    return;
                }
                m_serialManager->setOutputSignals(m_dtrSetCheck->isChecked(), m_rtsSetCheck->isChecked()); });

    m_signalPollTimer = new QTimer(this);
    m_signalPollTimer->setInterval(400);
    connect(m_signalPollTimer, &QTimer::timeout, this, [this]()
            {
                if (!m_serialManager)
                {
                    return;
                }
                const auto s = m_serialManager->hardwareSignals();
                m_dtrLabel->setText(QString("DTR: %1").arg(s.dtr ? "ON" : "OFF"));
                m_rtsLabel->setText(QString("RTS: %1").arg(s.rts ? "ON" : "OFF"));
                m_ctsLabel->setText(QString("CTS: %1").arg(s.cts ? "ON" : "OFF"));
                m_dsrLabel->setText(QString("DSR: %1").arg(s.dsr ? "ON" : "OFF")); });
    m_signalPollTimer->start();

    return w;
}

QWidget *AdvancedLabPlugin::buildSimulationTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);

    m_simProfileCombo = new QComboBox(w);
    m_simProfileCombo->addItems({"SIM:Echo", "SIM:AT", "SIM:Modbus"});
    m_simStatusLabel = new QLabel(QString::fromUtf8("状态: 未连接"), w);
    auto *connectBtn = new QPushButton(QString::fromUtf8("连接仿真"), w);
    auto *disconnectBtn = new QPushButton(QString::fromUtf8("断开仿真"), w);

    layout->addWidget(new QLabel(QString::fromUtf8("仿真设备配置"), w));
    layout->addWidget(m_simProfileCombo);
    layout->addWidget(connectBtn);
    layout->addWidget(disconnectBtn);
    layout->addWidget(m_simStatusLabel);
    layout->addStretch();

    connect(connectBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_serialManager)
                {
                    return;
                }
                ISerialTransport::Config cfg;
                cfg.portName = m_simProfileCombo->currentText();
                cfg.baudRate = 115200;
                if (m_serialManager->connectPort(cfg))
                {
                    m_simStatusLabel->setText(QString::fromUtf8("状态: 已连接 ") + cfg.portName);
                } });

    connect(disconnectBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_serialManager)
                {
                    return;
                }
                m_serialManager->disconnectPort();
                m_simStatusLabel->setText(QString::fromUtf8("状态: 未连接")); });

    return w;
}

QWidget *AdvancedLabPlugin::buildFuzzTab()
{
    auto *w = new QWidget(this);
    auto *layout = new QVBoxLayout(w);
    auto *form = new QFormLayout();

    m_fuzzSeedEdit = new QLineEdit(w);
    m_fuzzSeedEdit->setPlaceholderText(QString::fromUtf8("种子帧 HEX，例如 01 03 00 00 00 01 84 0A"));
    m_fuzzLevelCombo = new QComboBox(w);
    m_fuzzLevelCombo->addItems({QString::fromUtf8("低"), QString::fromUtf8("中"), QString::fromUtf8("高")});
    m_fuzzCountEdit = new QLineEdit(w);
    m_fuzzCountEdit->setText("100");
    form->addRow(QString::fromUtf8("种子"), m_fuzzSeedEdit);
    form->addRow(QString::fromUtf8("变异等级"), m_fuzzLevelCombo);
    form->addRow(QString::fromUtf8("发送次数"), m_fuzzCountEdit);
    layout->addLayout(form);

    auto *startBtn = new QPushButton(QString::fromUtf8("开始 Fuzz"), w);
    auto *stopBtn = new QPushButton(QString::fromUtf8("停止"), w);
    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(startBtn);
    btnRow->addWidget(stopBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_fuzzStatsLabel = new QLabel(QString::fromUtf8("已发送: 0, 收到响应: 0"), w);
    layout->addWidget(m_fuzzStatsLabel);

    m_fuzzTimer = new QTimer(this);
    m_fuzzTimer->setInterval(20);
    connect(m_fuzzTimer, &QTimer::timeout, this, [this]()
            {
                if (!m_serialManager || !m_serialManager->isConnected() || m_fuzzRemain <= 0 || m_fuzzSeed.isEmpty())
                {
                    m_fuzzTimer->stop();
                    return;
                }
                const int level = m_fuzzLevelCombo ? m_fuzzLevelCombo->currentIndex() : 0;
                const QByteArray frame = mutateFrame(m_fuzzSeed, level);
                m_serialManager->sendData(frame);
                ++m_fuzzSent;
                --m_fuzzRemain;
                m_fuzzStatsLabel->setText(QString::fromUtf8("已发送: %1, 收到响应: %2").arg(m_fuzzSent).arg(m_fuzzResponses)); });

    connect(startBtn, &QPushButton::clicked, this, [this]()
            {
                bool ok = false;
                m_fuzzSeed = parseHex(m_fuzzSeedEdit->text(), &ok);
                if (!ok || m_fuzzSeed.isEmpty())
                {
                    QMessageBox::warning(this, QString::fromUtf8("Fuzz"), QString::fromUtf8("种子帧格式错误"));
                    return;
                }
                const int count = m_fuzzCountEdit->text().toInt(&ok);
                if (!ok || count <= 0)
                {
                    return;
                }
                m_fuzzRemain = count;
                m_fuzzSent = 0;
                m_fuzzResponses = 0;
                m_fuzzTimer->start(); });

    connect(stopBtn, &QPushButton::clicked, this, [this]()
            { m_fuzzTimer->stop(); });

    return w;
}

void AdvancedLabPlugin::onDataReceived(const QByteArray &data)
{
    if (data.isEmpty())
    {
        return;
    }

    if (m_reverseAutoCheck && m_reverseAutoCheck->isChecked())
    {
        analyzePayload(data);
    }

    if (!m_lastRxPayload.isEmpty() && m_lastRxPayload == data)
    {
        ++m_retransFrames;
    }
    m_lastRxPayload = data;

    if (data.size() >= 2)
    {
        const QByteArray payload = data.left(data.size() - 2);
        const quint16 recvCrc = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + data.size() - 2));
        if (crc16Modbus(payload) != recvCrc)
        {
            ++m_errorFrames;
        }
    }

    processTriggerRules(data, true);

    ++m_fuzzResponses;
    if (m_fuzzStatsLabel)
    {
        m_fuzzStatsLabel->setText(QString::fromUtf8("已发送: %1, 收到响应: %2").arg(m_fuzzSent).arg(m_fuzzResponses));
    }

    updateTrafficUi();
}

void AdvancedLabPlugin::refreshMonitorPortCombos()
{
    if (!m_leftPortCombo || !m_rightPortCombo)
    {
        return;
    }

    const QString leftCurrent = m_leftPortCombo->currentText();
    const QString rightCurrent = m_rightPortCombo->currentText();

    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts())
    {
        ports.append(info.portName());
    }
    ports.append("SIM:Echo");
    ports.append("SIM:AT");
    ports.append("SIM:Modbus");
    ports.removeDuplicates();
    ports.sort();

    m_leftPortCombo->clear();
    m_rightPortCombo->clear();
    m_leftPortCombo->addItems(ports);
    m_rightPortCombo->addItems(ports);

    if (!leftCurrent.isEmpty())
    {
        m_leftPortCombo->setCurrentText(leftCurrent);
    }
    if (!rightCurrent.isEmpty())
    {
        m_rightPortCombo->setCurrentText(rightCurrent);
    }
}

void AdvancedLabPlugin::processTriggerRules(const QByteArray &data, bool isReceive)
{
    if (data.isEmpty())
    {
        return;
    }

    const QString text = QString::fromUtf8(data);
    const QString hex = QString::fromUtf8(data.toHex(' ').toUpper());
    const QString normalizedHex = QString(hex).remove(' ');

    for (const TriggerRule &rule : std::as_const(m_triggers))
    {
        if (!rule.enabled || rule.keyword.isEmpty())
        {
            continue;
        }

        const QString key = rule.keyword.trimmed();
        const QString keyHex = QString(key).remove(' ').toUpper();
        const bool hitText = text.contains(key, Qt::CaseInsensitive);
        const bool hitHex = !keyHex.isEmpty() && normalizedHex.contains(keyHex, Qt::CaseInsensitive);
        if (!hitText && !hitHex)
        {
            continue;
        }

        if (m_triggerList)
        {
            m_triggerList->addItem(QString::fromUtf8("%1命中: %2 @ %3 | 数据=%4")
                                       .arg(isReceive ? "RX " : "TX ",
                                            rule.keyword,
                                            QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                                            hex));
        }

        if (isReceive && !rule.response.isEmpty() && m_serialManager && m_serialManager->isConnected())
        {
            m_serialManager->sendData(rule.response.toUtf8());
            if (m_triggerList)
            {
                m_triggerList->addItem(QString::fromUtf8("自动响应: %1").arg(rule.response));
            }
        }

        if (isReceive && m_triggerAlertCheck && m_triggerAlertCheck->isChecked())
        {
            QMessageBox::warning(this, QString::fromUtf8("触发器告警"), QString::fromUtf8("命中关键字: ") + rule.keyword);
        }
    }
}

void AdvancedLabPlugin::updateTrafficUi()
{
    if (m_errorFrameLabel)
    {
        m_errorFrameLabel->setText(QString::fromUtf8("错误帧: %1").arg(m_errorFrames));
    }
    const double ratio = (m_fuzzSent == 0) ? 0.0 : (100.0 * static_cast<double>(m_retransFrames) / static_cast<double>(m_fuzzSent));
    if (m_retransLabel)
    {
        m_retransLabel->setText(QString::fromUtf8("重传率: %1%")
                                    .arg(QString::number(ratio, 'f', 2)));
    }
}

void AdvancedLabPlugin::analyzePayload(const QByteArray &data)
{
    if (!m_reverseHints)
    {
        return;
    }
    const QString hint = runProtocolHeuristics(data);
    m_reverseHints->addItem(QString("[%1] %2")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), hint));
    while (m_reverseHints->count() > 200)
    {
        delete m_reverseHints->takeItem(0);
    }
}

QString AdvancedLabPlugin::runProtocolHeuristics(const QByteArray &data) const
{
    int slip = 0;
    int hdlc = 0;
    int cobsLikely = 0;

    for (unsigned char b : data)
    {
        if (b == 0xC0)
        {
            ++slip;
        }
        if (b == 0x7E)
        {
            ++hdlc;
        }
        if (b == 0x00)
        {
            ++cobsLikely;
        }
    }

    QStringList guesses;
    if (slip >= 2)
    {
        guesses << QString::fromUtf8("SLIP 边界特征明显 (0xC0)");
    }
    if (hdlc >= 2)
    {
        guesses << QString::fromUtf8("HDLC 帧界定符明显 (0x7E)");
    }
    if (cobsLikely <= 1)
    {
        guesses << QString::fromUtf8("COBS 可能性较高 (低零字节密度)");
    }
    if (guesses.isEmpty())
    {
        guesses << QString::fromUtf8("未识别典型帧边界，建议尝试固定帧长/CRC 逆向");
    }

    return guesses.join(" | ");
}

QByteArray AdvancedLabPlugin::parseHex(const QString &text, bool *ok) const
{
    if (ok)
    {
        *ok = false;
    }
    QString normalized = text;
    normalized.replace(QRegularExpression("[\\r\\n,;]+"), " ");
    const QStringList parts = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return {};
    }

    QByteArray out;
    out.reserve(parts.size());
    for (const QString &p : parts)
    {
        bool localOk = false;
        const int v = p.toInt(&localOk, 16);
        if (!localOk || v < 0 || v > 255)
        {
            return {};
        }
        out.append(static_cast<char>(v));
    }
    if (ok)
    {
        *ok = true;
    }
    return out;
}

QByteArray AdvancedLabPlugin::mutateFrame(const QByteArray &seed, int level) const
{
    QByteArray out = seed;
    const int mutations = (level + 1) * 2;
    quint32 state = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
    for (int i = 0; i < mutations && !out.isEmpty(); ++i)
    {
        state = state * 1664525u + 1013904223u;
        const int idx = static_cast<int>(state % static_cast<quint32>(out.size()));
        state = state * 1664525u + 1013904223u;
        const int delta = static_cast<int>(state & 0xFFu);
        out[idx] = static_cast<char>(static_cast<unsigned char>(out.at(idx)) ^ static_cast<unsigned char>(delta));
    }
    return out;
}
