#include "SerialBox/plugin/BerTesterPlugin.h"
#include "SerialBox/core/SerialPortManager.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>
#include <bit>

BerTesterPlugin::BerTesterPlugin(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void BerTesterPlugin::setSerialManager(SerialPortManager *manager)
{
    if (m_rxConn)
    {
        QObject::disconnect(m_rxConn);
    }
    if (m_disconnectedConn)
    {
        QObject::disconnect(m_disconnectedConn);
    }

    m_serialManager = manager;
    if (!m_serialManager)
    {
        return;
    }

    m_rxConn = connect(m_serialManager, &SerialPortManager::dataReceived,
                       this, &BerTesterPlugin::onDataReceived);
    m_disconnectedConn = connect(m_serialManager, &SerialPortManager::disconnected, this, [this]()
                                 { stopTxLoop(); });
}

void BerTesterPlugin::setupUi()
{
    setWindowTitle("插件: 误码率测试");
    setMinimumSize(600, 380);
    setWindowFlags(Qt::Window);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItems({"文本模式", "HEX模式", "递增模式"});

    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setPlaceholderText("文本示例: TEST123");

    m_repeatSpin = new QSpinBox(this);
    m_repeatSpin->setRange(1, 1000000);
    m_repeatSpin->setValue(100);

    m_incrementStartSpin = new QSpinBox(this);
    m_incrementStartSpin->setRange(0, 255);
    m_incrementStartSpin->setValue(0);

    m_incrementLenSpin = new QSpinBox(this);
    m_incrementLenSpin->setRange(1, 2048);
    m_incrementLenSpin->setValue(16);

    form->addRow("模式", m_modeCombo);
    form->addRow("期望帧", m_patternEdit);
    form->addRow("循环次数", m_repeatSpin);
    form->addRow("递增起点", m_incrementStartSpin);
    form->addRow("递增长度", m_incrementLenSpin);
    layout->addLayout(form);

    auto updateModeUi = [this]()
    {
        const int mode = m_modeCombo ? m_modeCombo->currentIndex() : 0;
        const bool manualInput = mode == 0 || mode == 1;
        if (m_patternEdit)
        {
            m_patternEdit->setEnabled(manualInput);
            m_patternEdit->setPlaceholderText(
                mode == 0 ? "文本示例: TEST123" : (mode == 1 ? "HEX示例: AA 55 00 FF" : "递增模式自动生成测试帧"));
        }
        if (m_incrementStartSpin)
        {
            m_incrementStartSpin->setEnabled(mode == 2);
        }
        if (m_incrementLenSpin)
        {
            m_incrementLenSpin->setEnabled(mode == 2);
        }
    };
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [updateModeUi](int)
            { updateModeUi(); });
    updateModeUi();

    auto *btnRow = new QHBoxLayout();
    m_sendToggleBtn = new QPushButton("开始10ms发送", this);
    m_startStopBtn = new QPushButton("开始统计", this);
    auto *resetBtn = new QPushButton("清零", this);
    btnRow->addWidget(m_sendToggleBtn);
    btnRow->addWidget(m_startStopBtn);
    btnRow->addWidget(resetBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    auto *statsTitle = new QLabel("统计", this);
    statsTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(statsTitle);

    m_txFramesLabel = new QLabel("发送帧数: 0", this);
    m_txBytesLabel = new QLabel("发送字节: 0", this);
    m_txFailLabel = new QLabel("发送失败: 0", this);
    m_txRemainLabel = new QLabel("剩余循环: 0", this);
    m_totalBitsLabel = new QLabel("总比特: 0", this);
    m_errorBitsLabel = new QLabel("错误比特: 0", this);
    m_berLabel = new QLabel("BER: 0", this);
    m_rxBytesLabel = new QLabel("接收字节: 0", this);
    layout->addWidget(m_txFramesLabel);
    layout->addWidget(m_txBytesLabel);
    layout->addWidget(m_txFailLabel);
    layout->addWidget(m_txRemainLabel);
    layout->addWidget(m_totalBitsLabel);
    layout->addWidget(m_errorBitsLabel);
    layout->addWidget(m_berLabel);
    layout->addWidget(m_rxBytesLabel);
    layout->addStretch();

    m_txTimer = new QTimer(this);
    m_txTimer->setInterval(10);
    connect(m_txTimer, &QTimer::timeout, this, &BerTesterPlugin::sendOneFrame);

    connect(m_sendToggleBtn, &QPushButton::clicked, this, [this]()
            {
        if (!m_serialManager || !m_serialManager->isConnected()) {
            QMessageBox::information(this, "误码率插件", "请先连接串口。");
            return;
        }
        bool ok = false;
        QByteArray frame = currentPattern(&ok);
        if (!ok || frame.isEmpty()) {
            QMessageBox::warning(this, "误码率插件", "测试帧格式无效。");
            return;
        }

        if (!m_txRunning) {
            resetTxStats();
            m_txFrame = frame;
            m_txRemainCycles = static_cast<quint64>(m_repeatSpin ? m_repeatSpin->value() : 1);
            m_txTimer->start();
            m_txRunning = true;
            m_sendToggleBtn->setText("停止10ms发送");
            updateStatsUi();
        } else {
            stopTxLoop();
        } });

    connect(m_startStopBtn, &QPushButton::clicked, this, [this]()
            {
        if (!m_running) {
            bool ok = false;
            m_expected = currentPattern(&ok);
            if (!ok || m_expected.isEmpty()) {
                QMessageBox::warning(this, "误码率插件", "请先配置合法的期望帧。");
                return;
            }
            resetStats();
            m_running = true;
            m_startStopBtn->setText("停止统计");
        } else {
            m_running = false;
            m_startStopBtn->setText("开始统计");
            QMessageBox::information(this, "误码率测试结果", statsSummary());
        } });

    connect(resetBtn, &QPushButton::clicked, this, [this]()
            { resetStats(); });
}

QByteArray BerTesterPlugin::currentPattern(bool *ok) const
{
    if (ok)
        *ok = false;

    const int mode = m_modeCombo ? m_modeCombo->currentIndex() : 0;
    if (mode == 2)
    {
        QByteArray out;
        const int n = m_incrementLenSpin ? m_incrementLenSpin->value() : 16;
        const int start = m_incrementStartSpin ? m_incrementStartSpin->value() : 0;
        out.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            out.append(static_cast<char>((start + i) & 0xFF));
        }
        if (ok)
            *ok = true;
        return out;
    }

    const QString raw = m_patternEdit ? m_patternEdit->text().trimmed() : QString();
    if (raw.isEmpty())
    {
        return {};
    }

    if (mode == 1)
    {
        QString normalized = raw;
        normalized.replace(QRegularExpression("[\\r\\n,;]+"), " ");
        const QStringList parts = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            return {};
        }

        QByteArray out;
        out.reserve(parts.size());
        for (const QString &part : parts)
        {
            bool localOk = false;
            int val = part.toInt(&localOk, 16);
            if (!localOk || val < 0 || val > 255)
            {
                return {};
            }
            out.append(static_cast<char>(val));
        }
        if (ok)
            *ok = true;
        return out;
    }

    if (ok)
        *ok = true;
    return raw.toUtf8();
}

void BerTesterPlugin::onDataReceived(const QByteArray &data)
{
    if (data.isEmpty())
    {
        return;
    }

    if (m_running || m_txRunning || isVisible())
    {
        emit berDataCaptured(data);
    }

    m_rxBytes += static_cast<quint64>(data.size());
    if (m_rxBytesLabel)
    {
        m_rxBytesLabel->setText(QString("接收字节: %1").arg(m_rxBytes));
    }

    if (!m_running || m_expected.isEmpty())
    {
        return;
    }

    const int n = data.size();
    m_totalBits += static_cast<quint64>(n) * 8ULL;

    for (int i = 0; i < n; ++i)
    {
        const unsigned char actual = static_cast<unsigned char>(data.at(i));
        const unsigned char expect = static_cast<unsigned char>(m_expected.at(i % m_expected.size()));
        const unsigned char diff = static_cast<unsigned char>(actual ^ expect);
        m_errorBits += static_cast<quint64>(std::popcount(diff));
    }

    updateStatsUi();
}

void BerTesterPlugin::resetStats()
{
    resetTxStats();
    m_totalBits = 0;
    m_errorBits = 0;
    m_rxBytes = 0;
    if (m_rxBytesLabel)
    {
        m_rxBytesLabel->setText("接收字节: 0");
    }
    updateStatsUi();
}

void BerTesterPlugin::updateStatsUi()
{
    if (m_txFramesLabel)
    {
        m_txFramesLabel->setText(QString("发送帧数: %1").arg(m_txFrames));
    }
    if (m_txBytesLabel)
    {
        m_txBytesLabel->setText(QString("发送字节: %1").arg(m_txBytes));
    }
    if (m_txFailLabel)
    {
        m_txFailLabel->setText(QString("发送失败: %1").arg(m_txFailures));
    }
    if (m_txRemainLabel)
    {
        m_txRemainLabel->setText(QString("剩余循环: %1").arg(m_txRemainCycles));
    }
    m_totalBitsLabel->setText(QString("总比特: %1").arg(m_totalBits));
    m_errorBitsLabel->setText(QString("错误比特: %1").arg(m_errorBits));
    const double ber = m_totalBits == 0 ? 0.0 : (static_cast<double>(m_errorBits) / static_cast<double>(m_totalBits));
    m_berLabel->setText(QString("BER: %1").arg(QString::number(ber, 'g', 8)));
}

QString BerTesterPlugin::statsSummary() const
{
    const double ber = m_totalBits == 0 ? 0.0 : (static_cast<double>(m_errorBits) / static_cast<double>(m_totalBits));
    return QString("发送帧数: %1\n发送字节: %2\n接收字节: %3\n总比特: %4\n错误比特: %5\nBER: %6")
        .arg(m_txFrames)
        .arg(m_txBytes)
        .arg(m_rxBytes)
        .arg(m_totalBits)
        .arg(m_errorBits)
        .arg(QString::number(ber, 'g', 8));
}

void BerTesterPlugin::resetTxStats()
{
    m_txFrames = 0;
    m_txBytes = 0;
    m_txFailures = 0;
    m_txRemainCycles = 0;
}

void BerTesterPlugin::stopTxLoop()
{
    if (m_txTimer)
    {
        m_txTimer->stop();
    }
    m_txRunning = false;
    if (m_sendToggleBtn)
    {
        m_sendToggleBtn->setText("开始10ms发送");
    }
    updateStatsUi();
}

void BerTesterPlugin::sendOneFrame()
{
    if (!m_serialManager || !m_serialManager->isConnected() || m_txFrame.isEmpty())
    {
        stopTxLoop();
        return;
    }
    if (m_txRemainCycles == 0)
    {
        stopTxLoop();
        return;
    }

    const qint64 written = m_serialManager->sendData(m_txFrame);
    if (written > 0)
    {
        ++m_txFrames;
        m_txBytes += static_cast<quint64>(written);
    }
    else
    {
        ++m_txFailures;
    }

    if (m_txRemainCycles > 0)
    {
        --m_txRemainCycles;
    }
    updateStatsUi();

    if (m_txRemainCycles == 0)
    {
        stopTxLoop();
    }
}
