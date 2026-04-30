#pragma once
#include <QWidget>

class QTextEdit;
class QComboBox;
class QPushButton;
class QTimer;
class QSpinBox;
class DataPipeline;
class SerialPortManager;

/**
 * SendPanel — 发送区
 *
 * - 文本/HEX 双模式
 * - 动态变量替换 (${timestamp} 等)
 * - 追加换行符控制
 * - 发送历史
 */
class SendPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SendPanel(QWidget *parent = nullptr);

    void setPipeline(DataPipeline *pipeline);
    void setSerialManager(SerialPortManager *manager);

public slots:
    void setText(const QString &text);
    void sendCurrentText();
    void sendCommandDirect(const QString &text);
    void setSessionRecording(bool recording);

signals:
    void sessionRecordToggled(bool recording);

private:
    void setupUi();
    QString processVariables(const QString &text);
    void showHistoryMenu();
    QByteArray parseHexPayload(const QString &text, bool *ok) const;
    void setHexSendMode(bool enabled);

    QTextEdit *m_input = nullptr;
    QPushButton *m_textSendModeBtn = nullptr;
    QPushButton *m_hexSendModeBtn = nullptr;
    QComboBox *m_newlineCombo = nullptr;
    QPushButton *m_sessionRecordBtn = nullptr;
    QPushButton *m_historyBtn = nullptr;
    QPushButton *m_autoSendBtn = nullptr;
    QSpinBox *m_autoIntervalSpin = nullptr;
    QTimer *m_autoSendTimer = nullptr;
    DataPipeline *m_pipeline = nullptr;
    SerialPortManager *m_serialManager = nullptr;
    bool m_hexSendMode = false;
    bool m_sessionRecording = false;
    QStringList m_history;
    int m_historyIndex = -1;
};
