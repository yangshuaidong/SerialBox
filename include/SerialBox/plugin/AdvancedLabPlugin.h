#pragma once

#include <QDialog>
#include <QDateTime>
#include <QMetaObject>
#include <QVector>

class SerialPortManager;
class ConfigManager;
class QTabWidget;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QCheckBox;
class QPushButton;
class QTimer;
class QWidget;
class QSerialPort;
class SparklineWidget;

class AdvancedLabPlugin : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedLabPlugin(QWidget *parent = nullptr);

    void setSerialManager(SerialPortManager *manager);
    void setPluginContext(ConfigManager *config);
    void showBuiltinPluginTab();

signals:
    void templateCommandRequested(const QString &cmd);
    void builtinPluginToggled(const QString &pluginId, bool enabled);
    void builtinPluginOpenRequested(const QString &pluginId);

private:
    struct TriggerRule
    {
        QString keyword;
        QString response;
        bool enabled = true;
    };

    void setupUi();
    QWidget *buildMultiSerialTab();
    QWidget *buildReverseAssistTab();
    QWidget *buildTrafficTab();
    QWidget *buildTriggerTab();
    QWidget *buildConvertTab();
    QWidget *buildCrcTab();
    QWidget *buildPluginTab();
    QWidget *buildTemplateTab();
    QWidget *buildSignalTab();
    QWidget *buildSimulationTab();
    QWidget *buildFuzzTab();

    void refreshMonitorPortCombos();
    void processTriggerRules(const QByteArray &data, bool isReceive);
    void onDataReceived(const QByteArray &data);
    void updateTrafficUi();
    void analyzePayload(const QByteArray &data);
    QString runProtocolHeuristics(const QByteArray &data) const;
    QByteArray parseHex(const QString &text, bool *ok) const;
    QByteArray mutateFrame(const QByteArray &seed, int level) const;

    SerialPortManager *m_serialManager = nullptr;
    ConfigManager *m_config = nullptr;
    QMetaObject::Connection m_rxConn;
    QMetaObject::Connection m_txConn;
    QMetaObject::Connection m_statsConn;

    QTabWidget *m_tabs = nullptr;

    // 2. 多串口监控
    QComboBox *m_leftPortCombo = nullptr;
    QComboBox *m_rightPortCombo = nullptr;
    QPlainTextEdit *m_leftMonitorLog = nullptr;
    QPlainTextEdit *m_rightMonitorLog = nullptr;
    QSerialPort *m_leftMonitorPort = nullptr;
    QSerialPort *m_rightMonitorPort = nullptr;

    // 3. 协议逆向辅助
    QPlainTextEdit *m_reverseInput = nullptr;
    QListWidget *m_reverseHints = nullptr;
    QCheckBox *m_reverseAutoCheck = nullptr;

    // 4. 流量统计
    QLabel *m_rxRateLabel = nullptr;
    QLabel *m_txRateLabel = nullptr;
    QLabel *m_errorFrameLabel = nullptr;
    QLabel *m_retransLabel = nullptr;
    SparklineWidget *m_rxSpark = nullptr;
    SparklineWidget *m_txSpark = nullptr;
    quint64 m_prevRxBytes = 0;
    quint64 m_prevTxBytes = 0;
    quint64 m_errorFrames = 0;
    quint64 m_retransFrames = 0;
    QByteArray m_lastRxPayload;

    // 6. 触发器
    QLineEdit *m_triggerKeywordEdit = nullptr;
    QLineEdit *m_triggerResponseEdit = nullptr;
    QListWidget *m_triggerList = nullptr;
    QCheckBox *m_triggerAlertCheck = nullptr;
    QVector<TriggerRule> m_triggers;

    // 7. 转换管道
    QLineEdit *m_convertHexInput = nullptr;
    QLineEdit *m_convertBase64Output = nullptr;
    QLineEdit *m_convertUtf16Output = nullptr;
    QLineEdit *m_convertCrc16Output = nullptr;
    QLineEdit *m_convertCrc32Output = nullptr;

    // CRC 校验
    QComboBox *m_crcAlgoCombo = nullptr;
    QComboBox *m_crcEndianCombo = nullptr;
    QLineEdit *m_crcCalcInput = nullptr;
    QLineEdit *m_crcCalcOutput = nullptr;
    QLineEdit *m_crcVerifyInput = nullptr;
    QLabel *m_crcVerifyResult = nullptr;

    // 插件管理
    QCheckBox *m_builtinBerCheck = nullptr;
    QCheckBox *m_builtinWaveCheck = nullptr;
    QCheckBox *m_builtinReportCheck = nullptr;

    // 8. 模板市场
    QListWidget *m_templateList = nullptr;

    // 11. 硬件信号监控
    QLabel *m_dtrLabel = nullptr;
    QLabel *m_rtsLabel = nullptr;
    QLabel *m_ctsLabel = nullptr;
    QLabel *m_dsrLabel = nullptr;
    QCheckBox *m_dtrSetCheck = nullptr;
    QCheckBox *m_rtsSetCheck = nullptr;
    QTimer *m_signalPollTimer = nullptr;

    // 12. 仿真模式
    QComboBox *m_simProfileCombo = nullptr;
    QLabel *m_simStatusLabel = nullptr;

    // 13. Fuzzing
    QLineEdit *m_fuzzSeedEdit = nullptr;
    QComboBox *m_fuzzLevelCombo = nullptr;
    QLineEdit *m_fuzzCountEdit = nullptr;
    QLabel *m_fuzzStatsLabel = nullptr;
    QTimer *m_fuzzTimer = nullptr;
    QByteArray m_fuzzSeed;
    int m_fuzzRemain = 0;
    quint64 m_fuzzSent = 0;
    quint64 m_fuzzResponses = 0;

    QDateTime m_lastStatsTs;
};