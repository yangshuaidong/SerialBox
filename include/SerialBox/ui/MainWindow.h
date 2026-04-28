#pragma once
#include <QMainWindow>
#include <QSplitter>
#include <QByteArray>
#include <QDateTime>
#include <QVector>

class QAction;
class SerialPortManager;
class ConfigManager;
class DataPipeline;
class ReceivePanel;
class SendPanel;
class CommandSidebar;
class WaveformWidget;
class SearchBar;
class StatusBar;
class SerialDialog;
class SerialSettings;
class QTimer;
class QResizeEvent;
class BerTesterPlugin;
class TestReportPlugin;
class AdvancedLabPlugin;
class PythonEngine;
class ScriptRunner;

/**
 * MainWindow — 主窗口
 *
 * 布局：
 * ┌──────────────────────────────────┐
 * │ 菜单栏                            │
 * │ 工具栏                            │
 * │ ┌──────────────┐ ┌────────────┐ │
 * │ │ 接收区 (3/4) │ │ 命令库侧栏 │ │
 * │ │              │ │            │ │
 * │ ├──────────────┤ │            │ │
 * │ │ 发送区 (1/4) │ │            │ │
 * │ └──────────────┘ └────────────┘ │
 * │ 状态栏                            │
 * └──────────────────────────────────┘
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setManagers(SerialPortManager *serial, ConfigManager *config,
                     DataPipeline *pipeline);

public slots:
    void showSerialDialog();
    void refreshPortList(const QStringList &ports);
    void appendReceiveData(const QByteArray &data, bool isReceive, const QDateTime &ts);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void toggleCommandSidebar();
    void toggleWaveform();
    void switchWorkMode(int mode);

private:
    struct SessionFrame
    {
        qint64 offsetMs = 0;
        bool isReceive = true;
        QByteArray data;
    };

    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void loadSettings();
    void saveSettings();
    void setSearchBarVisible(bool visible);
    void positionSearchBar();
    bool isBuiltinPluginEnabled(const QString &pluginId) const;
    void setBuiltinPluginEnabled(const QString &pluginId, bool enabled);
    void startSessionRecording();
    void stopSessionRecordingAndExport();
    void replaySessionFromFile();
    void cancelReplay();
    bool exportSessionJson(const QString &path) const;
    bool exportSessionCsv(const QString &path) const;
    bool exportSessionPcap(const QString &path) const;
    bool loadSessionJson(const QString &path, QVector<SessionFrame> &frames, QDateTime &baseTime, QString *error = nullptr) const;
    void replayFrames(const QVector<SessionFrame> &frames, const QDateTime &baseTime);
    void saveSessionSnapshot();
    void loadSessionSnapshot();
    void setupPythonEngine();

    // ── 子面板 ──
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_contentSplitter = nullptr;
    ReceivePanel *m_receivePanel = nullptr;
    SendPanel *m_sendPanel = nullptr;
    CommandSidebar *m_commandSidebar = nullptr;
    WaveformWidget *m_waveformWidget = nullptr;
    SearchBar *m_searchBar = nullptr;
    StatusBar *m_statusBar = nullptr;

    // ── 弹窗 ──
    SerialDialog *m_serialDialog = nullptr;
    SerialSettings *m_serialSettings = nullptr;
    BerTesterPlugin *m_berTester = nullptr;
    TestReportPlugin *m_reportPlugin = nullptr;
    AdvancedLabPlugin *m_advancedLab = nullptr;

#ifdef ENABLE_PYTHON
    PythonEngine *m_pythonEngine = nullptr;
    ScriptRunner *m_scriptRunner = nullptr;
#endif

    // ── 管理器引用 ──
    SerialPortManager *m_serialManager = nullptr;
    ConfigManager *m_config = nullptr;
    DataPipeline *m_pipeline = nullptr;

    // ── 菜单/工具栏 Action（同步选中态） ──
    QAction *m_cmdMenuAction = nullptr;
    QAction *m_waveMenuAction = nullptr;
    QAction *m_cmdToolbarAction = nullptr;
    QAction *m_waveToolbarAction = nullptr;
    QAction *m_textModeAction = nullptr;
    QAction *m_hexModeAction = nullptr;
    QAction *m_timestampAction = nullptr;
    QAction *m_echoAction = nullptr;
    QAction *m_searchMenuAction = nullptr;
    QAction *m_searchToolbarAction = nullptr;

    QTimer *m_autoSendTimer = nullptr;

    bool m_sessionRecording = false;
    bool m_sessionReplaying = false;
    qint64 m_replayPendingEvents = 0;
    QDateTime m_recordingStartTime;
    QVector<SessionFrame> m_recordedFrames;
    QList<QMetaObject::Connection> m_replayConnections;
};
