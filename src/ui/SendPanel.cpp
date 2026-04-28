#include "SerialBox/ui/SendPanel.h"
#include "SerialBox/core/DataPipeline.h"
#include "SerialBox/core/SerialPortManager.h"

#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QKeyEvent>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QMenu>
#include <QSpinBox>
#include <QTimer>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStyle>

SendPanel::SendPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SendPanel::setPipeline(DataPipeline *pipeline) { m_pipeline = pipeline; }
void SendPanel::setSerialManager(SerialPortManager *manager) { m_serialManager = manager; }

void SendPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // 输入框
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText("输入要发送的数据...\n支持 ${timestamp}、${counter} 等动态变量\nHEX 模式: 01 03 00 00 00 01 84 0A");
    layout->addWidget(m_input, 1);

    // 控制栏
    auto *controls = new QHBoxLayout();
    controls->setSpacing(6);

    // 发送模式按钮（互斥高亮）
    m_textSendModeBtn = new QPushButton("文本发送", this);
    m_textSendModeBtn->setObjectName("sendModeTextBtn");
    m_textSendModeBtn->setCheckable(true);
    m_textSendModeBtn->setProperty("sendMode", "text");
    controls->addWidget(m_textSendModeBtn);

    m_hexSendModeBtn = new QPushButton("HEX发送", this);
    m_hexSendModeBtn->setObjectName("sendModeHexBtn");
    m_hexSendModeBtn->setCheckable(true);
    m_hexSendModeBtn->setProperty("sendMode", "hex");
    controls->addWidget(m_hexSendModeBtn);

    connect(m_textSendModeBtn, &QPushButton::clicked, this, [this]()
            { setHexSendMode(false); });
    connect(m_hexSendModeBtn, &QPushButton::clicked, this, [this]()
            { setHexSendMode(true); });
    setHexSendMode(false);

    m_newlineCombo = new QComboBox(this);
    m_newlineCombo->setObjectName("sendNewlineCombo");
    m_newlineCombo->addItems({"追加 \\r\\n", "追加 \\n", "无"});
    controls->addWidget(m_newlineCombo);

    auto *hint = new QLabel("Enter 发送 · Shift+Enter 换行", this);
    controls->addWidget(hint);
    controls->addStretch();

    m_sessionRecordBtn = new QPushButton("会话录制", this);
    m_sessionRecordBtn->setObjectName("sessionRecordBtn");
    m_sessionRecordBtn->setCheckable(true);
    m_sessionRecordBtn->setProperty("role", "warning");
    connect(m_sessionRecordBtn, &QPushButton::toggled, this, [this](bool checked)
            {
                setSessionRecording(checked);
                emit sessionRecordToggled(checked); });
    controls->addWidget(m_sessionRecordBtn);

    // 历史
    m_historyBtn = new QPushButton("历史", this);
    m_historyBtn->setObjectName("sendHistoryBtn");
    m_historyBtn->setProperty("role", "info");
    connect(m_historyBtn, &QPushButton::clicked, this, [this]()
            { showHistoryMenu(); });
    controls->addWidget(m_historyBtn);

    m_autoIntervalSpin = new QSpinBox(this);
    m_autoIntervalSpin->setObjectName("sendIntervalSpin");
    m_autoIntervalSpin->setRange(10, 3600000);
    m_autoIntervalSpin->setValue(1000);
    m_autoIntervalSpin->setSuffix(" ms");
    m_autoIntervalSpin->setKeyboardTracking(false);
    m_autoIntervalSpin->setAccelerated(true);
    controls->addWidget(m_autoIntervalSpin);

    m_autoSendBtn = new QPushButton("开始自动发送", this);
    m_autoSendBtn->setObjectName("sendAutoBtn");
    m_autoSendBtn->setProperty("role", "warning");
    controls->addWidget(m_autoSendBtn);

    // 发送按钮
    auto *sendBtn = new QPushButton("发送", this);
    sendBtn->setObjectName("sendNowBtn");
    sendBtn->setProperty("role", "primary");
    connect(sendBtn, &QPushButton::clicked, this, &SendPanel::sendCurrentText);
    controls->addWidget(sendBtn);

    layout->addLayout(controls);

    m_autoSendTimer = new QTimer(this);
    connect(m_autoSendTimer, &QTimer::timeout, this, &SendPanel::sendCurrentText);
    connect(m_autoSendBtn, &QPushButton::clicked, this, [this]()
            {
        if (m_autoSendTimer->isActive()) {
            m_autoSendTimer->stop();
            m_autoSendBtn->setText("开始自动发送");
            m_autoSendBtn->setProperty("role", "warning");
        } else {
            m_autoSendTimer->start(m_autoIntervalSpin->value());
            m_autoSendBtn->setText("停止自动发送");
            m_autoSendBtn->setProperty("role", "success");
        }
        m_autoSendBtn->style()->unpolish(m_autoSendBtn);
        m_autoSendBtn->style()->polish(m_autoSendBtn);
        m_autoSendBtn->update(); });
}

void SendPanel::setText(const QString &text)
{
    m_input->setPlainText(text);
    m_input->setFocus();
}

QString SendPanel::processVariables(const QString &text)
{
    QString result = text;
    result.replace("${timestamp}", QString::number(QDateTime::currentSecsSinceEpoch()));
    result.replace("${datetime}", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    result.replace("${date}", QDate::currentDate().toString("yyyy-MM-dd"));
    result.replace("${time}", QTime::currentTime().toString("HH:mm:ss"));
    // ${counter} 需要持久化计数器
    return result;
}

void SendPanel::sendCurrentText()
{
    QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    // 变量替换
    text = processVariables(text);

    // 记录历史（去重，保留最近 50 条）
    const QString rawText = m_input->toPlainText();
    m_history.removeAll(rawText);
    m_history.append(rawText);
    while (m_history.size() > 50)
    {
        m_history.removeFirst();
    }
    m_historyIndex = m_history.size();

    // 按选项追加换行
    if (m_newlineCombo)
    {
        if (m_newlineCombo->currentIndex() == 0)
        {
            text += "\r\n";
        }
        else if (m_newlineCombo->currentIndex() == 1)
        {
            text += "\n";
        }
    }

    QByteArray data;
    if (m_hexSendMode)
    {
        bool ok = false;
        data = parseHexPayload(text, &ok);
        if (!ok)
        {
            QMessageBox::warning(this, "HEX 格式错误", "请输入十六进制字节序列，例如: 01 03 00 00 00 01 84 0A");
            return;
        }
    }
    else
    {
        data = text.toUtf8();
    }

    if (m_pipeline)
    {
        data = m_pipeline->processOutgoing(data);
    }

    if (!m_serialManager || !m_serialManager->isConnected())
    {
        QMessageBox::information(this, "发送失败", "串口未连接，数据未发送。");
        return;
    }

    const qint64 written = m_serialManager->sendData(data);
    if (written <= 0)
    {
        QMessageBox::warning(this, "发送失败", "串口写入失败，数据未发送。");
        return;
    }
}

void SendPanel::sendCommandDirect(const QString &text)
{
    setText(text);
    sendCurrentText();
}

void SendPanel::setSessionRecording(bool recording)
{
    m_sessionRecording = recording;
    if (!m_sessionRecordBtn)
    {
        return;
    }

    {
        const QSignalBlocker blocker(m_sessionRecordBtn);
        m_sessionRecordBtn->setChecked(recording);
    }
    m_sessionRecordBtn->setText(recording ? "结束录制并导出" : "会话录制");
    m_sessionRecordBtn->setProperty("role", recording ? "danger" : "warning");
    m_sessionRecordBtn->style()->unpolish(m_sessionRecordBtn);
    m_sessionRecordBtn->style()->polish(m_sessionRecordBtn);
    m_sessionRecordBtn->update();
}

void SendPanel::setHexSendMode(bool enabled)
{
    m_hexSendMode = enabled;

    if (m_textSendModeBtn)
    {
        m_textSendModeBtn->setChecked(!enabled);
        m_textSendModeBtn->setProperty("active", !enabled);
        m_textSendModeBtn->style()->unpolish(m_textSendModeBtn);
        m_textSendModeBtn->style()->polish(m_textSendModeBtn);
        m_textSendModeBtn->update();
    }
    if (m_hexSendModeBtn)
    {
        m_hexSendModeBtn->setChecked(enabled);
        m_hexSendModeBtn->setProperty("active", enabled);
        m_hexSendModeBtn->style()->unpolish(m_hexSendModeBtn);
        m_hexSendModeBtn->style()->polish(m_hexSendModeBtn);
        m_hexSendModeBtn->update();
    }

    if (m_input)
    {
        if (enabled)
        {
            m_input->setPlaceholderText("HEX发送模式\n示例: 01 03 00 00 00 01 84 0A");
        }
        else
        {
            m_input->setPlaceholderText("文本发送模式\n支持 ${timestamp}、${counter} 等动态变量");
        }
    }
}

void SendPanel::showHistoryMenu()
{
    if (!m_historyBtn)
        return;

    QMenu menu(this);
    if (m_history.isEmpty())
    {
        QAction *emptyAction = menu.addAction("暂无历史");
        emptyAction->setEnabled(false);
    }
    else
    {
        for (int i = m_history.size() - 1; i >= 0; --i)
        {
            QString preview = m_history.at(i);
            preview.replace('\n', " ");
            if (preview.size() > 36)
            {
                preview = preview.left(36) + "...";
            }
            QAction *act = menu.addAction(preview);
            connect(act, &QAction::triggered, this, [this, i]()
                    { setText(m_history.at(i)); });
        }
        menu.addSeparator();
        QAction *clearAction = menu.addAction("清空历史");
        connect(clearAction, &QAction::triggered, this, [this]()
                {
            m_history.clear();
            m_historyIndex = -1; });
    }

    menu.exec(m_historyBtn->mapToGlobal(QPoint(0, m_historyBtn->height())));
}

QByteArray SendPanel::parseHexPayload(const QString &text, bool *ok) const
{
    if (ok)
        *ok = false;
    QString normalized = text;
    normalized.replace(QRegularExpression("[\\r\\n,;]+"), " ");
    QStringList parts = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return {};
    }

    QByteArray out;
    out.reserve(parts.size());
    for (const QString &p : parts)
    {
        bool localOk = false;
        int v = p.toInt(&localOk, 16);
        if (!localOk || v < 0 || v > 255)
        {
            return {};
        }
        out.append(static_cast<char>(v));
    }
    if (ok)
        *ok = true;
    return out;
}
