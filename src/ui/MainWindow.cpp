#include "SerialBox/ui/MainWindow.h"
#include "SerialBox/ui/ReceivePanel.h"
#include "SerialBox/ui/SendPanel.h"
#include "SerialBox/ui/CommandSidebar.h"
#include "SerialBox/ui/WaveformWidget.h"
#include "SerialBox/ui/SearchBar.h"
#include "SerialBox/ui/StatusBar.h"
#include "SerialBox/ui/SerialDialog.h"
#include "SerialBox/ui/SerialSettings.h"
#include "SerialBox/ui/WorkspaceModes.h"
#include "SerialBox/ui/CommandSidebar.h"

#ifdef ENABLE_PYTHON
#include "SerialBox/python/PythonEngine.h"
#include "SerialBox/python/ScriptRunner.h"
#endif
#include "SerialBox/plugin/BerTesterPlugin.h"
#include "SerialBox/plugin/TestReportPlugin.h"
#include "SerialBox/plugin/AdvancedLabPlugin.h"
#include "SerialBox/core/SerialPortManager.h"
#include "SerialBox/core/DataPipeline.h"
#include "SerialBox/app/ConfigManager.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QDir>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QDataStream>
#include <functional>
#include <algorithm>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QPointer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SerialBox");
    resize(1200, 800);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();

    // 弹窗
    m_serialDialog = new SerialDialog(this);
    m_serialSettings = new SerialSettings(this);

    // 波形图 — 独立窗口（不设 parent，可与主窗口同步操作）
    m_waveformWidget = new WaveformWidget(nullptr);
    m_berTester = new BerTesterPlugin(nullptr);
    m_reportPlugin = new TestReportPlugin(nullptr);
    m_advancedLab = new AdvancedLabPlugin(nullptr);

    // Python 引擎
    setupPythonEngine();

    connect(m_serialDialog, &SerialDialog::connectRequested,
            this, &MainWindow::onConnectClicked);
    connect(m_serialDialog, &SerialDialog::advancedSettingsRequested,
            this, [this]()
            {
                if (m_serialSettings && m_config) {
                    m_serialSettings->refreshFromConfig(m_config);
                    m_serialSettings->show();
                    m_serialSettings->raise();
                    m_serialSettings->activateWindow();
                } });
    connect(m_serialSettings, &SerialSettings::settingsApplied,
            this, &MainWindow::onConnectClicked);

    m_autoSendTimer = new QTimer(this);
    connect(m_autoSendTimer, &QTimer::timeout, this, &MainWindow::onSendClicked);
}

MainWindow::~MainWindow()
{
    saveSettings();
    if (m_waveformWidget)
    {
        m_waveformWidget->close(); // 先关闭窗口，断开与主窗口的同步操作
        delete m_waveformWidget;
    }
    if (m_berTester)
    {
        m_berTester->close();
        delete m_berTester;
    }
    if (m_reportPlugin)
    {
        m_reportPlugin->close();
        delete m_reportPlugin;
    }
    if (m_advancedLab)
    {
        m_advancedLab->close();
        delete m_advancedLab;
    }
#ifdef ENABLE_PYTHON
    if (m_scriptRunner) {
        if (m_scriptRunner->isRunning()) m_scriptRunner->stop();
        delete m_scriptRunner;
    }
    if (m_pythonEngine) {
        m_pythonEngine->shutdown();
        delete m_pythonEngine;
    }
#endif
}

void MainWindow::setupPythonEngine()
{
#ifdef ENABLE_PYTHON
    m_pythonEngine = new PythonEngine(this);
    m_scriptRunner = new ScriptRunner(m_pythonEngine, this);

    // 连接脚本输出到接收区
    connect(m_scriptRunner, &ScriptRunner::output, this, [this](const QString &text) {
        if (m_receivePanel) {
            m_receivePanel->appendSystemNote(QString("[Python] %1").arg(text), true);
        }
    });
    connect(m_scriptRunner, &ScriptRunner::error, this, [this](const QString &text) {
        if (m_receivePanel) {
            m_receivePanel->appendSystemNote(QString("[Python 错误] %1").arg(text), false);
        }
    });
    connect(m_scriptRunner, &ScriptRunner::finished, this, [this](int code) {
        if (m_receivePanel) {
            m_receivePanel->appendSystemNote(
                QString("Python 脚本结束 (退出码: %1)").arg(code), code == 0);
        }
    });

    // 连接串口数据到 Python 回调
    if (m_serialManager) {
        connect(m_serialManager, &SerialPortManager::dataReceived, this,
                [this](const QByteArray &data) {
                    if (m_pythonEngine && m_pythonEngine->isInitialized()) {
                        m_pythonEngine->feedDataToCallbacks(
                            QString::fromUtf8(data));
                    }
                });
    }

    // 初始化 Python 引擎
    if (!m_pythonEngine->initialize()) {
        qWarning("Python 引擎初始化失败");
    }

    // 默认模块白名单
    m_scriptRunner->setAllowedModules(
        {"math", "json", "time", "struct", "collections", "serialbox", "re", "base64", "hashlib"});
#endif
}

void MainWindow::setManagers(SerialPortManager *serial, ConfigManager *config,
                             DataPipeline *pipeline)
{
    m_serialManager = serial;
    m_config = config;
    m_pipeline = pipeline;

    m_receivePanel->setPipeline(pipeline);
    m_sendPanel->setPipeline(pipeline);
    m_sendPanel->setSerialManager(serial);
    connect(m_sendPanel, &SendPanel::sessionRecordToggled, this, [this](bool recording)
            {
                if (recording) {
                    startSessionRecording();
                } else {
                    stopSessionRecordingAndExport();
                } });
    m_statusBar->setSerialManager(serial);
    m_serialDialog->setConfigManager(config);
    m_serialSettings->updatePortList(QStringList{});
    m_serialSettings->refreshFromConfig(config);
    m_commandSidebar->setCommandLibrary(config->commandLibrary());
    if (m_berTester)
    {
        m_berTester->setSerialManager(serial);
        connect(m_berTester, &BerTesterPlugin::berDataCaptured, this, [this](const QByteArray &data)
                {
                    if (!m_receivePanel || data.isEmpty()) {
                        return;
                    }
                    QString preview = QString(data.toHex(' ').toUpper());
                    if (preview.size() > 120) {
                        preview = preview.left(120) + " ...";
                    }
                    m_receivePanel->appendSystemNote(QString("BER接收: %1").arg(preview), true); });
    }
    if (m_reportPlugin)
    {
        m_reportPlugin->setSerialManager(serial);
    }
    if (m_advancedLab)
    {
        m_advancedLab->setSerialManager(serial);
        m_advancedLab->setPluginContext(m_config);
        connect(m_advancedLab, &AdvancedLabPlugin::templateCommandRequested,
                m_sendPanel, &SendPanel::setText, Qt::UniqueConnection);
        connect(m_advancedLab, &AdvancedLabPlugin::builtinPluginToggled, this,
                [this](const QString &pluginId, bool enabled)
                {
                    setBuiltinPluginEnabled(pluginId, enabled);
                });
        connect(m_advancedLab, &AdvancedLabPlugin::builtinPluginOpenRequested, this,
                [this](const QString &pluginId)
                {
                    if (pluginId == "builtin.ber" && m_berTester)
                    {
                        if (!isBuiltinPluginEnabled(pluginId))
                            return;
                        m_berTester->show();
                        m_berTester->raise();
                        m_berTester->activateWindow();
                    }
                    else if (pluginId == "builtin.waveform" && m_waveformWidget)
                    {
                        if (!isBuiltinPluginEnabled(pluginId))
                            return;
                        m_waveformWidget->show();
                        m_waveformWidget->raise();
                        m_waveformWidget->activateWindow();
                    }
                    else if (pluginId == "builtin.report" && m_reportPlugin)
                    {
                        if (!isBuiltinPluginEnabled(pluginId))
                            return;
                        m_reportPlugin->show();
                        m_reportPlugin->raise();
                        m_reportPlugin->activateWindow();
                    }
                });
    }

    if (m_waveformWidget && !isBuiltinPluginEnabled("builtin.waveform"))
    {
        m_waveformWidget->hide();
    }
    if (m_berTester && !isBuiltinPluginEnabled("builtin.ber"))
    {
        m_berTester->hide();
    }
    if (m_reportPlugin && !isBuiltinPluginEnabled("builtin.report"))
    {
        m_reportPlugin->hide();
    }
    if (m_advancedLab && !isBuiltinPluginEnabled("builtin.advanced"))
    {
        m_advancedLab->hide();
    }

    connect(m_commandSidebar, &CommandSidebar::commandLibraryChanged,
            this, [this](const QJsonObject &lib)
            {
                if (!m_config) return;
                m_config->setCommandLibrary(lib);
                m_config->save(); });

    connect(m_pipeline, &DataPipeline::displayModeChanged,
            m_receivePanel, &ReceivePanel::refreshDisplayMode, Qt::UniqueConnection);

    // 连接信号
    connect(serial, &SerialPortManager::connected, this, [this]()
            { m_statusBar->setStatusMessage("已连接", true); });
    connect(serial, &SerialPortManager::disconnected, this, [this]()
            { m_statusBar->setStatusMessage("已断开", false); });
    connect(serial, &SerialPortManager::errorOccurred, this, [this](const QString &err)
            { m_statusBar->setStatusMessage("错误: " + err, false); });
    connect(serial, &SerialPortManager::reconnecting, this, [this](int attempt, int maxRetries)
            { m_statusBar->setStatusMessage(QString("重连中 %1/%2").arg(attempt).arg(maxRetries), false); });
    connect(serial, &SerialPortManager::reconnected, this, [this]()
            { m_statusBar->setStatusMessage("已重连", true); });
    loadSettings();
}

// ═══════════════════════════════════════════
//  菜单栏
// ═══════════════════════════════════════════
void MainWindow::setupMenuBar()
{
    // ── 文件 ──
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("打开日志(&O)...", QKeySequence::Open, [this]()
                        {
        const QString path = QFileDialog::getOpenFileName(this, "打开日志", QString(), "日志文件 (*.log *.txt);;所有文件 (*)");
        if (path.isEmpty()) return;
        QMessageBox::information(this, "打开日志", QString("已选择日志文件:\n%1").arg(path)); });
    fileMenu->addAction("保存接收区(&S)", QKeySequence::Save, [this]()
                        {
        const QString path = QFileDialog::getSaveFileName(this, "保存接收区", "receive.txt", "文本文件 (*.txt)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "保存失败", "无法写入目标文件。");
            return;
        }
        QTextStream out(&f);
        out << m_receivePanel->toPlainText();
        QMessageBox::information(this, "保存成功", "接收区内容已导出。"); });
    fileMenu->addSeparator();
    fileMenu->addAction("开始会话录制", [this]()
                        { startSessionRecording(); });
    fileMenu->addAction("停止录制并导出...", [this]()
                        { stopSessionRecordingAndExport(); });
    fileMenu->addAction("回放会话...", [this]()
                        { replaySessionFromFile(); });
    fileMenu->addAction("停止回放", [this]()
                        { cancelReplay(); });
    fileMenu->addSeparator();
    fileMenu->addAction("导入预设命令...", [this]()
                        {
        const QString path = QFileDialog::getOpenFileName(this, "导入预设命令", QString(), "JSON 文件 (*.json)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "导入失败", "无法读取文件。");
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) {
            QMessageBox::warning(this, "导入失败", "JSON 格式无效。请先使用“导出预设命令模板...”生成模板后编辑导入。");
            return;
        }
        QJsonObject commandObj = doc.object();
        if (commandObj.contains("commands") && commandObj.value("commands").isObject()) {
            commandObj = commandObj.value("commands").toObject();
        }
        if (!commandObj.contains("custom") || !commandObj.value("custom").isArray()) {
            QMessageBox::warning(this, "导入失败",
                "缺少 custom 数组。\n模板结构应为:\n{\n  \"custom\": [\n    {\"name\": \"命令名\", \"cmd\": \"命令内容\"}\n  ]\n}");
            return;
        }
        const QJsonArray arr = commandObj.value("custom").toArray();
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonObject obj = arr.at(i).toObject();
            if (!obj.contains("name") || !obj.contains("cmd")) {
                QMessageBox::warning(this, "导入失败",
                    QString("第 %1 条命令缺少 name 或 cmd 字段。\n请检查模板后重试。").arg(i + 1));
                return;
            }
        }
        m_commandSidebar->setCommandLibrary(commandObj);
        if (m_config) {
            m_config->setCommandLibrary(commandObj);
            m_config->save();
        }
        QMessageBox::information(this, "导入成功", "命令库已更新。"); });
    fileMenu->addAction("导出预设命令模板...", [this]()
                        {
        const QString path = QFileDialog::getSaveFileName(this, "导出预设命令模板", "commands-template.json", "JSON 文件 (*.json)");
        if (path.isEmpty()) return;

        QJsonArray custom;
        QJsonObject cmd1;
        cmd1.insert("name", "心跳包");
        cmd1.insert("cmd", "PING_${timestamp}");
        custom.append(cmd1);

        QJsonObject cmd2;
        cmd2.insert("name", "读寄存器");
        cmd2.insert("cmd", "01 03 00 00 00 01 84 0A");
        custom.append(cmd2);

        QJsonObject root;
        root.insert("custom", custom);

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "导出失败", "无法写入模板文件。");
            return;
        }
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        QMessageBox::information(this, "导出成功", "模板已导出，可按模板编辑后导入。"); });
    fileMenu->addAction("导出配置...", [this]()
                        {
        const QString path = QFileDialog::getSaveFileName(this, "导出配置", "serialbox-config.json", "JSON 文件 (*.json)");
        if (path.isEmpty()) return;
        QJsonObject obj;
        if (m_config) {
            const auto preset = m_config->serialPreset();
            QJsonObject serial;
            serial.insert("port", preset.portName);
            serial.insert("baud", preset.baudRate);
            serial.insert("dataBits", preset.dataBits);
            serial.insert("stopBits", preset.stopBits);
            serial.insert("parity", preset.parity);
            serial.insert("flow", preset.flowControl);
            serial.insert("timeout", preset.timeoutMs);
            obj.insert("serial", serial);
            obj.insert("commands", m_commandSidebar->commandLibrary());
        }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "导出失败", "无法写入文件。");
            return;
        }
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        QMessageBox::information(this, "导出成功", "配置已导出。"); });
    fileMenu->addSeparator();
    fileMenu->addAction("保存会话快照...", [this]()
                        { saveSessionSnapshot(); });
    fileMenu->addAction("加载会话快照...", [this]()
                        { loadSessionSnapshot(); });
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&X)", QKeySequence::Quit, this, &QWidget::close);

    // ── 串口 ──
    QMenu *serialMenu = menuBar()->addMenu("串口(&S)");
    serialMenu->addAction("串口设置(&P)...", QKeySequence("Ctrl+P"), this,
                          &MainWindow::showSerialDialog);
    serialMenu->addAction("连接/断开(&D)", QKeySequence("Ctrl+D"), this, [this]()
                          {
        if (m_serialManager->isConnected()) onDisconnectClicked();
        else showSerialDialog(); });
    serialMenu->addSeparator();
    serialMenu->addAction("刷新端口列表", [this]()
                          { m_serialManager->refreshPorts(); });
    serialMenu->addSeparator();
    serialMenu->addAction("断线重连设置...", [this]()
                          {
        QDialog dialog(this);
        dialog.setWindowTitle("断线重连设置");
        auto *form = new QFormLayout(&dialog);
        auto *enable = new QCheckBox("启用自动重连", &dialog);
        auto *retrySpin = new QSpinBox(&dialog);
        retrySpin->setRange(1, 99);
        retrySpin->setValue(5);
        auto *intervalSpin = new QSpinBox(&dialog);
        intervalSpin->setRange(200, 60000);
        intervalSpin->setValue(3000);
        intervalSpin->setSuffix(" ms");
        form->addRow(enable);
        form->addRow("最大重试次数", retrySpin);
        form->addRow("重试间隔", intervalSpin);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted && m_serialManager) {
            m_serialManager->setAutoReconnect(enable->isChecked(), retrySpin->value(), intervalSpin->value());
            QMessageBox::information(this, "设置成功", "断线重连策略已生效。");
        } });

    // ── 视图 —— checkable 带 ✓ 提示 ──
    QMenu *viewMenu = menuBar()->addMenu("视图(&V)");

    m_searchMenuAction = viewMenu->addAction("搜索");
    m_searchMenuAction->setShortcut(QKeySequence("Ctrl+F"));
    m_searchMenuAction->setCheckable(true);
    connect(m_searchMenuAction, &QAction::triggered, this, [this](bool checked)
            { setSearchBarVisible(checked); });
    viewMenu->addSeparator();
    viewMenu->addAction("清空接收区", [this]()
                        { m_receivePanel->clear(); });
    viewMenu->addAction("切换书签", [this]()
                        {
        if (m_receivePanel) m_receivePanel->toggleBookmarkCurrent(); });
    viewMenu->addAction("下一个书签", [this]()
                        {
        if (m_receivePanel) m_receivePanel->jumpNextBookmark(); });
    viewMenu->addAction("上一个书签", [this]()
                        {
        if (m_receivePanel) m_receivePanel->jumpPrevBookmark(); });
    viewMenu->addSeparator();

    QMenu *themeMenu = viewMenu->addMenu("主题");
    auto applyTheme = [this](const QString &themeId, const QString &qssPath, bool isCustomPath)
    {
        QFile f(qssPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "主题", "主题文件无法打开。");
            return;
        }
        qApp->setStyleSheet(QTextStream(&f).readAll());
        if (m_config)
        {
            m_config->setTheme(themeId);
            if (isCustomPath)
            {
                m_config->setCustomThemePath(qssPath);
            }
            m_config->save();
        }
    };

    themeMenu->addAction("浅色", [applyTheme]()
                         { applyTheme("light", ":/themes/light.qss", false); });
    themeMenu->addAction("暗色", [applyTheme]()
                         { applyTheme("dark", ":/themes/dark.qss", false); });
    themeMenu->addAction("专业高对比", [applyTheme]()
                         { applyTheme("professional", ":/themes/professional.qss", false); });
    themeMenu->addAction("色盲友好（Ocean）", [applyTheme]()
                         { applyTheme("ocean", ":/themes/ocean.qss", false); });
    themeMenu->addAction("高对比石墨（Graphite）", [applyTheme]()
                         { applyTheme("graphite", ":/themes/graphite.qss", false); });
    themeMenu->addSeparator();
    themeMenu->addAction("字体增大", [this]()
                         {
        QFont f = qApp->font();
        f.setPointSize(qMin(28, f.pointSize() + 1));
        qApp->setFont(f); });
    themeMenu->addAction("字体减小", [this]()
                         {
        QFont f = qApp->font();
        f.setPointSize(qMax(8, f.pointSize() - 1));
        qApp->setFont(f); });

    // ── 工具 ──
    QMenu *toolMenu = menuBar()->addMenu("工具(&T)");
    toolMenu->addAction("定时自动发送...", [this]()
                        {
        bool ok = false;
        QString intervalText = QInputDialog::getText(this, "定时自动发送", "发送间隔 (ms)", QLineEdit::Normal, "1000", &ok);
        if (!ok) return;
        bool numberOk = false;
        int interval = intervalText.trimmed().toInt(&numberOk);
        if (!numberOk || interval < 10 || interval > 3600000) {
            QMessageBox::warning(this, "定时发送", "请输入 10 到 3600000 的毫秒值。");
            return;
        }
        if (m_autoSendTimer->isActive()) {
            m_autoSendTimer->stop();
            QMessageBox::information(this, "定时发送", "已关闭定时自动发送。");
        } else {
            m_autoSendTimer->start(interval);
            QMessageBox::information(this, "定时发送", QString("已开启，间隔 %1 ms。").arg(interval));
        } });
    toolMenu->addAction("流量统计...", [this]()
                        {
        if (!m_serialManager) return;
        const auto st = m_serialManager->stats();
        QMessageBox::information(this, "流量统计",
            QString("接收: %1 B\n发送: %2 B\n连接时长: %3 s")
                .arg(st.rxBytes)
                .arg(st.txBytes)
                .arg(st.connectedMs / 1000)); });
    toolMenu->addAction("高级调试中心...", [this]()
                        {
        if (!m_advancedLab) return;
        m_advancedLab->show();
        m_advancedLab->raise();
        m_advancedLab->activateWindow(); });
#ifdef ENABLE_PYTHON
    toolMenu->addSeparator();
    toolMenu->addAction("运行 Python 脚本...", [this]()
                        {
        if (!m_pythonEngine || !m_pythonEngine->isInitialized()) {
            QMessageBox::warning(this, "Python", "Python 引擎未初始化。");
            return;
        }
        const QString path = QFileDialog::getOpenFileName(this, "选择 Python 脚本",
            QString(), "Python 文件 (*.py);;所有文件 (*)");
        if (path.isEmpty()) return;

        if (m_scriptRunner && m_scriptRunner->isRunning()) {
            auto ret = QMessageBox::question(this, "Python",
                "当前有脚本正在运行，是否停止后执行新脚本？",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                m_scriptRunner->stop();
            } else {
                return;
            }
        }

        m_scriptRunner->run(QString("exec(open('%1').read())").arg(path));
        if (m_receivePanel) {
            m_receivePanel->appendSystemNote(QString("执行 Python 脚本: %1").arg(path), true);
        } });
    toolMenu->addAction("停止 Python 脚本", [this]()
                        {
        if (m_scriptRunner && m_scriptRunner->isRunning()) {
            m_scriptRunner->stop();
            if (m_receivePanel) {
                m_receivePanel->appendSystemNote("Python 脚本已停止", false);
            }
        } });
#endif
    toolMenu->addSeparator();
    toolMenu->addAction("内置插件...", [this]()
                        {
        if (!m_advancedLab) return;
        m_advancedLab->show();
        m_advancedLab->raise();
        m_advancedLab->activateWindow();
        m_advancedLab->showBuiltinPluginTab(); });

    // ── 帮助 ──
    QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("快捷键列表", [this]()
                        { QMessageBox::information(this, "快捷键列表",
                                                   "Ctrl+P  串口设置\n"
                                                   "Ctrl+D  连接/断开\n"
                                                   "Ctrl+F  搜索\n"
                                                   "Ctrl+S  保存接收区\n"
                                                   "F2      命令库\n"
                                                   "F3      波形图\n"
                                                   "Enter   发送(发送区按钮)\n"); });
    helpMenu->addAction("插件开发文档", [this]()
                        {
        QFile f(":/docs/README_PLUGIN_DEV.md");
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "插件开发文档", "未找到 README_PLUGIN_DEV.md");
            return;
        }
        auto *dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle("插件开发文档");
        dlg->resize(760, 560);
        auto *layout = new QVBoxLayout(dlg);
        auto *viewer = new QTextBrowser(dlg);
        viewer->setPlainText(QString::fromUtf8(f.readAll()));
        layout->addWidget(viewer);
        dlg->show(); });
    helpMenu->addAction("关于 SerialBox", [this]()
                        { QMessageBox::about(this, "关于 SerialBox",
                                             "<h3>SerialBox</h3>"
                                             "<p>版本 1.0.0</p>"
                                             "<p>现代化跨平台串口调试工具</p>"
                                             "<p>Built with Qt6</p>"); });

    // 菜单中带 ✓ 的样式
    menuBar()->setStyleSheet(menuBar()->styleSheet() +
                             "QMenu::indicator { width: 0; height: 0; }" // 隐藏默认 checkbox 方块
                             "QMenu::item:checked { "
                             "  color: #4A90D9; "
                             "  font-weight: bold; "
                             "}");
}

// ═══════════════════════════════════════════
//  工具栏
// ═══════════════════════════════════════════
void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar("主工具栏");
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));

    // 连接/断开
    QAction *connectAction = tb->addAction("连接", this, &MainWindow::onConnectClicked);
    QAction *disconnectAction = tb->addAction("断开", this, &MainWindow::onDisconnectClicked);
    if (QWidget *w = tb->widgetForAction(connectAction))
    {
        w->setObjectName("toolConnectBtn");
    }
    if (QWidget *w = tb->widgetForAction(disconnectAction))
    {
        w->setObjectName("toolDisconnectBtn");
    }
    tb->addSeparator();

    // 串口参数
    tb->addAction("串口设置", this, &MainWindow::showSerialDialog);
    tb->addSeparator();

    // ── 显示模式 —— checkable, 互斥 ──
    m_textModeAction = tb->addAction("文本接收");
    m_textModeAction->setCheckable(true);
    m_textModeAction->setChecked(true);
    if (QWidget *w = tb->widgetForAction(m_textModeAction))
    {
        w->setObjectName("recvTextModeBtn");
    }
    connect(m_textModeAction, &QAction::triggered, this, [this](bool checked)
            {
        if (!checked) { m_textModeAction->setChecked(true); return; }
        m_hexModeAction->setChecked(false);
        m_pipeline->setDisplayMode(DataPipeline::DisplayMode::Text);
        m_textModeAction->setText("文本接收");
        m_hexModeAction->setText("HEX接收"); });

    m_hexModeAction = tb->addAction("HEX接收");
    m_hexModeAction->setCheckable(true);
    m_hexModeAction->setChecked(false);
    if (QWidget *w = tb->widgetForAction(m_hexModeAction))
    {
        w->setObjectName("recvHexModeBtn");
    }
    connect(m_hexModeAction, &QAction::triggered, this, [this](bool checked)
            {
        if (!checked) { m_hexModeAction->setChecked(true); return; }
        m_textModeAction->setChecked(false);
        m_pipeline->setDisplayMode(DataPipeline::DisplayMode::Hex);
        m_hexModeAction->setText("HEX接收");
        m_textModeAction->setText("文本接收"); });
    tb->addSeparator();

    // ── 辅助功能 —— checkable toggle ──
    m_timestampAction = tb->addAction("时间戳");
    m_timestampAction->setCheckable(true);
    m_timestampAction->setChecked(true);
    connect(m_timestampAction, &QAction::toggled, this, [this](bool on)
            {
        m_pipeline->setTimestampEnabled(on);
        m_timestampAction->setText(on ? "时间戳" : "时间戳"); });

    m_echoAction = tb->addAction("回显");
    m_echoAction->setCheckable(true);
    m_echoAction->setChecked(true);
    connect(m_echoAction, &QAction::toggled, this, [this](bool on)
            {
        m_pipeline->setEchoEnabled(on);
        m_echoAction->setText(on ? "回显" : "回显"); });

    m_searchToolbarAction = tb->addAction("搜索");
    m_searchToolbarAction->setCheckable(true);
    if (QWidget *w = tb->widgetForAction(m_searchToolbarAction))
    {
        w->setObjectName("toolSearchBtn");
    }
    connect(m_searchToolbarAction, &QAction::triggered, this, [this](bool checked)
            { setSearchBarVisible(checked); });
    tb->addAction("清空", [this]()
                  { m_receivePanel->clear(); });
    tb->addAction("保存接收", [this]()
                  {
        const QString path = QFileDialog::getSaveFileName(this, "保存接收区", "receive.txt", "文本文件 (*.txt)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "保存失败", "无法写入目标文件。");
            return;
        }
        QTextStream out(&f);
        out << m_receivePanel->toPlainText(); });
    tb->addSeparator();

    // ── 面板切换 —— checkable ──
    m_cmdToolbarAction = tb->addAction("命令库");
    m_cmdToolbarAction->setCheckable(true);
    m_cmdToolbarAction->setChecked(false);
    connect(m_cmdToolbarAction, &QAction::triggered, this, &MainWindow::toggleCommandSidebar);

    m_waveToolbarAction = tb->addAction("波形");
    m_waveToolbarAction->setCheckable(true);
    m_waveToolbarAction->setChecked(false);
    connect(m_waveToolbarAction, &QAction::triggered, this, &MainWindow::toggleWaveform);

    // 工作区模式
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
}

// ═══════════════════════════════════════════
//  中央区域
// ═══════════════════════════════════════════
void MainWindow::setupCentralWidget()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 悬浮搜索栏（初始隐藏）
    m_searchBar = new SearchBar(central);
    m_searchBar->setObjectName("searchFloatingBar");
    m_searchBar->hide();

    // 主分割器：内容区 | 侧边栏
    m_contentSplitter = new QSplitter(Qt::Horizontal, central);

    // 垂直分割器：接收区 | 发送区 (3:1)
    m_mainSplitter = new QSplitter(Qt::Vertical, m_contentSplitter);

    m_receivePanel = new ReceivePanel(m_mainSplitter);
    m_sendPanel = new SendPanel(m_mainSplitter);

    m_mainSplitter->addWidget(m_receivePanel);
    m_mainSplitter->addWidget(m_sendPanel);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);

    // 命令库侧边栏（初始隐藏）
    m_commandSidebar = new CommandSidebar(m_contentSplitter);
    m_commandSidebar->hide();

    m_contentSplitter->addWidget(m_mainSplitter);
    m_contentSplitter->addWidget(m_commandSidebar);
    m_contentSplitter->setStretchFactor(0, 1);
    m_contentSplitter->setStretchFactor(1, 0);

    layout->addWidget(m_contentSplitter);
    setCentralWidget(central);

    positionSearchBar();
    if (m_searchBar)
    {
        m_searchBar->raise();
    }

    // 搜索栏 → 接收区联动
    connect(m_searchBar, &SearchBar::searchRequested,
            m_receivePanel, &ReceivePanel::search);
    connect(m_searchBar, &SearchBar::searchNext,
            m_receivePanel, &ReceivePanel::searchNext);
    connect(m_searchBar, &SearchBar::searchPrev,
            m_receivePanel, &ReceivePanel::searchPrev);
    connect(m_searchBar, &SearchBar::closed,
            this, [this]()
            { setSearchBarVisible(false); });

    // 命令库 → 发送区联动
    connect(m_commandSidebar, &CommandSidebar::commandSelected,
            m_sendPanel, &SendPanel::sendCommandDirect);
}

// ═══════════════════════════════════════════
//  状态栏
// ═══════════════════════════════════════════
void MainWindow::setupStatusBar()
{
    m_statusBar = new StatusBar(this);
    setStatusBar(m_statusBar);
}

// ═══════════════════════════════════════════
//  槽函数
// ═══════════════════════════════════════════
void MainWindow::showSerialDialog()
{
    if (m_serialManager->isConnected())
    {
        m_serialSettings->refreshFromConfig(m_config);
        m_serialSettings->show();
    }
    else
    {
        m_serialDialog->refreshPorts();
        m_serialDialog->show();
    }
}

void MainWindow::refreshPortList(const QStringList &ports)
{
    m_serialDialog->updatePortList(ports);
    m_serialSettings->updatePortList(ports);
}

void MainWindow::appendReceiveData(const QByteArray &data, bool isReceive, const QDateTime &ts)
{
    if (!isReceive && !m_sessionRecording && !m_sessionReplaying)
    {
        startSessionRecording();
    }

    if (m_sessionRecording)
    {
        if (!m_recordingStartTime.isValid())
        {
            m_recordingStartTime = ts;
        }
        SessionFrame frame;
        frame.offsetMs = qMax<qint64>(0, m_recordingStartTime.msecsTo(ts));
        frame.isReceive = isReceive;
        frame.data = data;
        m_recordedFrames.append(std::move(frame));
    }

    m_receivePanel->appendData(data, isReceive, ts);
    if (m_waveformWidget)
    {
        m_waveformWidget->feedData(data);
    }
}

void MainWindow::onConnectClicked()
{
    auto preset = m_config->serialPreset();
    ISerialTransport::Config cfg;
    cfg.portName = preset.portName;
    cfg.baudRate = preset.baudRate;
    cfg.dataBits = preset.dataBits;
    cfg.stopBits = preset.stopBits;
    cfg.parity = preset.parity;
    cfg.flowControl = preset.flowControl;
    cfg.readTimeoutMs = preset.timeoutMs;

    if (!m_serialManager->connectPort(cfg))
    {
        QMessageBox::warning(this, "连接失败",
                             QString("无法连接到 %1\n%2")
                                 .arg(cfg.portName, m_serialManager->stats().rxBytes ? "" : "未知错误"));
    }
}

void MainWindow::onDisconnectClicked()
{
    m_serialManager->disconnectPort();
}

void MainWindow::onSendClicked()
{
    m_sendPanel->sendCurrentText();
}

void MainWindow::toggleCommandSidebar()
{
    bool visible = !m_commandSidebar->isVisible();
    m_commandSidebar->setVisible(visible);
    // 同步菜单和工具栏的选中态
    if (m_cmdMenuAction)
        m_cmdMenuAction->setChecked(visible);
    if (m_cmdToolbarAction)
    {
        m_cmdToolbarAction->setChecked(visible);
        m_cmdToolbarAction->setText("命令库");
    }
}

void MainWindow::toggleWaveform()
{
    if (!m_waveformWidget)
        return;
    if (!isBuiltinPluginEnabled("builtin.waveform"))
    {
        m_waveformWidget->hide();
        if (m_waveMenuAction)
            m_waveMenuAction->setChecked(false);
        if (m_waveToolbarAction)
        {
            m_waveToolbarAction->setChecked(false);
            m_waveToolbarAction->setText("波形");
        }
        return;
    }

    if (m_waveformWidget->isVisible())
    {
        m_waveformWidget->hide();
        if (m_waveMenuAction)
            m_waveMenuAction->setChecked(false);
        if (m_waveToolbarAction)
        {
            m_waveToolbarAction->setChecked(false);
            m_waveToolbarAction->setText("波形");
        }
    }
    else
    {
        m_waveformWidget->show();
        m_waveformWidget->raise();
        m_waveformWidget->activateWindow();
        if (m_waveMenuAction)
            m_waveMenuAction->setChecked(true);
        if (m_waveToolbarAction)
        {
            m_waveToolbarAction->setChecked(true);
            m_waveToolbarAction->setText("✓ 波形");
        }
    }
}

void MainWindow::switchWorkMode(int mode)
{
    // 基础模式: 简洁 UI，隐藏高级功能
    // 专家模式: 完整 UI，所有面板可见
    // 自动化模式: 命令库 + 定时发送

    switch (static_cast<WorkspaceModes::Mode>(mode)) {
    case WorkspaceModes::Mode::Basic:
        // 隐藏侧边栏和波形
        if (m_commandSidebar) m_commandSidebar->hide();
        if (m_waveformWidget) m_waveformWidget->hide();
        if (m_berTester) m_berTester->hide();
        if (m_reportPlugin) m_reportPlugin->hide();
        if (m_advancedLab) m_advancedLab->hide();
        if (m_autoSendTimer) m_autoSendTimer->stop();
        if (m_cmdToolbarAction) {
            m_cmdToolbarAction->setChecked(false);
            m_cmdToolbarAction->setText("命令库");
        }
        if (m_waveToolbarAction) {
            m_waveToolbarAction->setChecked(false);
            m_waveToolbarAction->setText("波形");
        }
        break;

    case WorkspaceModes::Mode::Expert:
        // 显示所有面板
        if (m_commandSidebar) {
            m_commandSidebar->show();
            if (m_cmdToolbarAction) {
                m_cmdToolbarAction->setChecked(true);
                m_cmdToolbarAction->setText("✓ 命令库");
            }
        }
        if (m_waveformWidget && isBuiltinPluginEnabled("builtin.waveform")) {
            m_waveformWidget->show();
            if (m_waveToolbarAction) {
                m_waveToolbarAction->setChecked(true);
                m_waveToolbarAction->setText("✓ 波形");
            }
        }
        break;

    case WorkspaceModes::Mode::Automation:
        // 显示命令库，开启定时发送
        if (m_commandSidebar) {
            m_commandSidebar->show();
            if (m_cmdToolbarAction) {
                m_cmdToolbarAction->setChecked(true);
                m_cmdToolbarAction->setText("✓ 命令库");
            }
        }
        // 如果定时器未运行且有合理间隔，自动启动
        if (m_autoSendTimer && !m_autoSendTimer->isActive()) {
            m_autoSendTimer->start(1000);
        }
        break;
    }

    // 持久化
    if (m_config) {
        m_config->setWorkMode(static_cast<ConfigManager::WorkMode>(mode));
        m_config->save();
    }
}

void MainWindow::startSessionRecording()
{
    if (m_sessionReplaying)
    {
        QMessageBox::warning(this, "会话录制", "当前正在回放，无法开始录制。");
        if (m_sendPanel)
        {
            m_sendPanel->setSessionRecording(false);
        }
        return;
    }
    if (m_sessionRecording)
    {
        QMessageBox::information(this, "会话录制", "会话录制已在进行中。");
        if (m_sendPanel)
        {
            m_sendPanel->setSessionRecording(true);
        }
        return;
    }

    m_recordedFrames.clear();
    m_recordingStartTime = QDateTime();
    m_sessionRecording = true;
    if (m_sendPanel)
    {
        m_sendPanel->setSessionRecording(true);
    }
    if (m_receivePanel)
    {
        m_receivePanel->appendSystemNote("会话录制已开始（含时间戳和收发方向）", true);
    }
}

void MainWindow::stopSessionRecordingAndExport()
{
    if (!m_sessionRecording)
    {
        QMessageBox::information(this, "会话录制", "当前没有正在进行的录制。");
        if (m_sendPanel)
        {
            m_sendPanel->setSessionRecording(false);
        }
        return;
    }

    m_sessionRecording = false;
    if (m_sendPanel)
    {
        m_sendPanel->setSessionRecording(false);
    }
    if (m_recordedFrames.isEmpty())
    {
        if (m_receivePanel)
        {
            m_receivePanel->appendSystemNote("会话录制已停止，但未捕获到收发数据", false);
        }
        QMessageBox::information(this, "会话录制", "录制已停止，但没有可导出的帧。");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "导出会话记录",
        QDir::homePath() + "/serial-session.json",
        "JSON 会话 (*.json);;CSV 文件 (*.csv);;PCAP 文件 (*.pcap)");
    if (path.isEmpty())
    {
        if (m_receivePanel)
        {
            m_receivePanel->appendSystemNote("录制数据已保留在内存，可稍后重新开始/停止后导出", false);
        }
        return;
    }

    bool ok = false;
    if (path.endsWith(".csv", Qt::CaseInsensitive))
    {
        ok = exportSessionCsv(path);
    }
    else if (path.endsWith(".pcap", Qt::CaseInsensitive))
    {
        ok = exportSessionPcap(path);
    }
    else
    {
        ok = exportSessionJson(path);
    }

    if (!ok)
    {
        QMessageBox::warning(this, "导出失败", "无法写入目标文件。");
        return;
    }

    if (m_receivePanel)
    {
        m_receivePanel->appendSystemNote(QString("会话导出成功：%1").arg(path), true);
    }
    QMessageBox::information(this, "导出成功", QString("已导出 %1 帧会话数据。\n%2").arg(m_recordedFrames.size()).arg(path));
}

bool MainWindow::exportSessionJson(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QJsonArray frames;
    const QDateTime base = m_recordingStartTime.isValid() ? m_recordingStartTime : QDateTime::currentDateTime();
    for (const SessionFrame &frame : m_recordedFrames)
    {
        QJsonObject obj;
        obj.insert("offsetMs", static_cast<qint64>(frame.offsetMs));
        obj.insert("timestamp", base.addMSecs(frame.offsetMs).toString(Qt::ISODateWithMs));
        obj.insert("direction", frame.isReceive ? "rx" : "tx");
        obj.insert("dataHex", QString(frame.data.toHex(' ').toUpper()));
        obj.insert("size", frame.data.size());
        frames.append(obj);
    }

    QJsonObject root;
    root.insert("schema", "serialbox.session.v1");
    root.insert("baseTime", base.toString(Qt::ISODateWithMs));
    root.insert("frames", frames);

    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool MainWindow::exportSessionCsv(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream out(&f);
    out << "offset_ms,direction,size,data_hex\n";
    for (const SessionFrame &frame : m_recordedFrames)
    {
        out << frame.offsetMs << ","
            << (frame.isReceive ? "rx" : "tx") << ","
            << frame.data.size() << ","
            << QString(frame.data.toHex(' ').toUpper()) << "\n";
    }
    return true;
}

bool MainWindow::exportSessionPcap(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        return false;
    }

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    // pcap 全局头；network=147 (DLT_USER0)，首字节编码方向（0=RX,1=TX）
    ds << static_cast<quint32>(0xa1b2c3d4);
    ds << static_cast<quint16>(2);
    ds << static_cast<quint16>(4);
    ds << static_cast<qint32>(0);
    ds << static_cast<quint32>(0);
    ds << static_cast<quint32>(65535);
    ds << static_cast<quint32>(147);

    const QDateTime base = m_recordingStartTime.isValid() ? m_recordingStartTime : QDateTime::currentDateTime();
    for (const SessionFrame &frame : m_recordedFrames)
    {
        const QDateTime frameTs = base.addMSecs(frame.offsetMs);
        const qint64 ms = frameTs.toMSecsSinceEpoch();
        const quint32 sec = static_cast<quint32>(ms / 1000);
        const quint32 usec = static_cast<quint32>((ms % 1000) * 1000);

        QByteArray packet;
        packet.reserve(frame.data.size() + 1);
        packet.append(frame.isReceive ? '\x00' : '\x01');
        packet.append(frame.data);

        const quint32 len = static_cast<quint32>(packet.size());
        ds << sec << usec << len << len;
        ds.writeRawData(packet.constData(), packet.size());
    }
    return ds.status() == QDataStream::Ok;
}

bool MainWindow::loadSessionJson(const QString &path, QVector<SessionFrame> &frames, QDateTime &baseTime, QString *error) const
{
    frames.clear();
    baseTime = QDateTime();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error)
            *error = "无法读取文件";
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        if (error)
            *error = "JSON 格式无效";
        return false;
    }

    const QJsonObject root = doc.object();
    baseTime = QDateTime::fromString(root.value("baseTime").toString(), Qt::ISODateWithMs);
    const QJsonArray arr = root.value("frames").toArray();
    if (arr.isEmpty())
    {
        if (error)
            *error = "会话中没有可回放数据";
        return false;
    }

    QDateTime firstFrameTs;
    for (const QJsonValue &v : arr)
    {
        const QJsonObject obj = v.toObject();
        QString hex = obj.value("dataHex").toString().trimmed();
        const QByteArray data = QByteArray::fromHex(hex.remove(' ').toLatin1());
        if (data.isEmpty() && !hex.isEmpty())
        {
            continue;
        }

        SessionFrame frame;
        const QDateTime frameTs = QDateTime::fromString(obj.value("timestamp").toString(), Qt::ISODateWithMs);
        if (obj.contains("offsetMs"))
        {
            frame.offsetMs = static_cast<qint64>(obj.value("offsetMs").toDouble());
        }
        else if (frameTs.isValid())
        {
            if (!firstFrameTs.isValid())
            {
                firstFrameTs = frameTs;
            }
            frame.offsetMs = qMax<qint64>(0, firstFrameTs.msecsTo(frameTs));
        }
        else
        {
            frame.offsetMs = 0;
        }
        frame.isReceive = obj.value("direction").toString().toLower() != "tx";
        frame.data = data;
        frames.append(std::move(frame));
    }

    if (!baseTime.isValid() && firstFrameTs.isValid())
    {
        baseTime = firstFrameTs;
    }

    std::sort(frames.begin(), frames.end(), [](const SessionFrame &a, const SessionFrame &b)
              { return a.offsetMs < b.offsetMs; });

    if (frames.isEmpty())
    {
        if (error)
            *error = "会话帧解析失败";
        return false;
    }
    return true;
}

void MainWindow::replaySessionFromFile()
{
    if (m_sessionRecording)
    {
        QMessageBox::warning(this, "会话回放", "当前正在录制，请先停止录制。");
        return;
    }
    if (m_sessionReplaying)
    {
        auto ret = QMessageBox::question(this, "会话回放",
            "会话回放正在进行中。\n是否停止当前回放？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            cancelReplay();
        }
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, "回放会话", QString(), "JSON 会话 (*.json)");
    if (path.isEmpty())
    {
        return;
    }

    QVector<SessionFrame> frames;
    QDateTime baseTime;
    QString err;
    if (!loadSessionJson(path, frames, baseTime, &err))
    {
        QMessageBox::warning(this, "会话回放", err);
        return;
    }
    replayFrames(frames, baseTime);
}

void MainWindow::replayFrames(const QVector<SessionFrame> &frames, const QDateTime &baseTime)
{
    if (!m_pipeline || frames.isEmpty())
    {
        return;
    }

    // 取消之前的回放
    cancelReplay();

    m_sessionReplaying = true;
    m_replayPendingEvents = frames.size();
    const QDateTime safeBase = baseTime.isValid() ? baseTime : QDateTime::currentDateTime();

    if (m_receivePanel)
    {
        m_receivePanel->appendSystemNote(QString("开始回放会话，共 %1 帧").arg(frames.size()), true);
    }

    for (const SessionFrame &frame : frames)
    {
        const qint64 delay = qMax<qint64>(0, frame.offsetMs);
        // 使用 QPointer 防止对象析构后回调
        QPointer<MainWindow> self(this);
        QTimer::singleShot(delay, this, [self, this, frame, safeBase]()
                           {
            if (!self || !m_sessionReplaying) return; // 已取消或已析构

            const QDateTime ts = safeBase.addMSecs(frame.offsetMs);
            appendReceiveData(frame.data, frame.isReceive, ts);

            m_replayPendingEvents = qMax<qint64>(0, m_replayPendingEvents - 1);
            if (m_replayPendingEvents == 0)
            {
                m_sessionReplaying = false;
                if (m_receivePanel)
                {
                    m_receivePanel->appendSystemNote("会话回放完成", true);
                }
            } });
    }
}

void MainWindow::cancelReplay()
{
    if (!m_sessionReplaying) return;

    m_sessionReplaying = false;
    m_replayPendingEvents = 0;
    if (m_receivePanel)
    {
        m_receivePanel->appendSystemNote("会话回放已取消", false);
    }
}

void MainWindow::saveSessionSnapshot()
{
    if (!m_config || !m_receivePanel)
    {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, "保存会话快照", "session-snapshot.json", "JSON 文件 (*.json)");
    if (path.isEmpty())
    {
        return;
    }

    QJsonObject root;
    const auto preset = m_config->serialPreset();
    QJsonObject serial;
    serial.insert("port", preset.portName);
    serial.insert("baud", preset.baudRate);
    serial.insert("dataBits", preset.dataBits);
    serial.insert("stopBits", preset.stopBits);
    serial.insert("parity", preset.parity);
    serial.insert("flow", preset.flowControl);
    serial.insert("timeout", preset.timeoutMs);
    root.insert("serial", serial);

    QJsonArray splitter;
    const auto sizes = m_mainSplitter ? m_mainSplitter->sizes() : QList<int>{};
    for (int s : sizes)
    {
        splitter.append(s);
    }
    root.insert("splitter", splitter);

    QJsonObject plugins;
    plugins.insert("waveform", isBuiltinPluginEnabled("builtin.waveform"));
    plugins.insert("ber", isBuiltinPluginEnabled("builtin.ber"));
    plugins.insert("report", isBuiltinPluginEnabled("builtin.report"));
    plugins.insert("advanced", isBuiltinPluginEnabled("builtin.advanced"));
    root.insert("plugins", plugins);
    root.insert("bookmarks", m_receivePanel->exportBookmarks());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "会话快照", "无法写入快照文件。");
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_receivePanel->appendSystemNote(QString("会话快照已保存: %1").arg(path), true);
}

void MainWindow::loadSessionSnapshot()
{
    if (!m_config || !m_receivePanel)
    {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, "加载会话快照", QString(), "JSON 文件 (*.json)");
    if (path.isEmpty())
    {
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "会话快照", "无法读取快照文件。");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        QMessageBox::warning(this, "会话快照", "快照格式无效。");
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject serial = root.value("serial").toObject();
    auto preset = m_config->serialPreset();
    preset.portName = serial.value("port").toString(preset.portName);
    preset.baudRate = serial.value("baud").toInt(preset.baudRate);
    preset.dataBits = serial.value("dataBits").toInt(preset.dataBits);
    preset.stopBits = serial.value("stopBits").toInt(preset.stopBits);
    preset.parity = serial.value("parity").toString(preset.parity);
    preset.flowControl = serial.value("flow").toString(preset.flowControl);
    preset.timeoutMs = serial.value("timeout").toInt(preset.timeoutMs);
    m_config->setSerialPreset(preset);

    const QJsonArray splitArr = root.value("splitter").toArray();
    if (m_mainSplitter && splitArr.size() == 2)
    {
        m_mainSplitter->setSizes({splitArr.at(0).toInt(600), splitArr.at(1).toInt(200)});
    }

    const QJsonObject plugins = root.value("plugins").toObject();
    setBuiltinPluginEnabled("builtin.waveform", plugins.value("waveform").toBool(true));
    setBuiltinPluginEnabled("builtin.ber", plugins.value("ber").toBool(true));
    setBuiltinPluginEnabled("builtin.report", plugins.value("report").toBool(true));
    setBuiltinPluginEnabled("builtin.advanced", plugins.value("advanced").toBool(true));

    m_receivePanel->importBookmarks(root.value("bookmarks").toArray());
    m_config->save();
    m_receivePanel->appendSystemNote(QString("会话快照已加载: %1").arg(path), true);
}

// ═══════════════════════════════════════════
//  配置
// ═══════════════════════════════════════════
void MainWindow::loadSettings()
{
    if (!m_config)
        return;
    restoreGeometry(m_config->windowGeometry());
    restoreState(m_config->windowState());
    m_mainSplitter->setSizes({600, 200});
}

void MainWindow::saveSettings()
{
    if (!m_config)
        return;
    m_config->setWindowGeometry(saveGeometry());
    m_config->setWindowState(saveState());
    auto sizes = m_mainSplitter->sizes();
    if (sizes.size() == 2 && sizes[1] > 0)
    {
        m_config->setSplitRatio(static_cast<double>(sizes[0]) / (sizes[0] + sizes[1]));
    }
    m_config->save();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();

    // 停止定时发送和回放
    if (m_autoSendTimer) m_autoSendTimer->stop();
    cancelReplay();
    m_sessionRecording = false;

    // 停止 Python 脚本
#ifdef ENABLE_PYTHON
    if (m_scriptRunner && m_scriptRunner->isRunning()) {
        m_scriptRunner->stop();
    }
    if (m_pythonEngine) {
        m_pythonEngine->shutdown();
    }
#endif

    if (m_serialManager && m_serialManager->isConnected())
    {
        m_serialManager->disconnectPort();
    }
    if (m_waveformWidget)
    {
        m_waveformWidget->close();
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionSearchBar();
}

void MainWindow::setSearchBarVisible(bool visible)
{
    if (!m_searchBar)
    {
        return;
    }

    if (visible)
    {
        positionSearchBar();
        m_searchBar->showAndFocus();
        m_searchBar->raise();
    }
    else
    {
        m_searchBar->hide();
        if (m_receivePanel)
        {
            m_receivePanel->clearSearchHighlight();
        }
    }

    if (m_searchMenuAction)
    {
        const QSignalBlocker blocker(m_searchMenuAction);
        m_searchMenuAction->setChecked(visible);
    }
    if (m_searchToolbarAction)
    {
        const QSignalBlocker blocker(m_searchToolbarAction);
        m_searchToolbarAction->setChecked(visible);
    }
}

void MainWindow::positionSearchBar()
{
    if (!m_searchBar || !centralWidget())
    {
        return;
    }

    const int availableWidth = qMax(180, centralWidget()->width() - 16);
    int targetWidth = qMin(860, qMax(360, availableWidth));
    targetWidth = qMin(targetWidth, availableWidth);
    m_searchBar->adjustSize();
    const int targetHeight = qMax(44, m_searchBar->sizeHint().height());
    m_searchBar->setFixedWidth(targetWidth);
    m_searchBar->setFixedHeight(targetHeight);
    const int x = qMax(8, (centralWidget()->width() - targetWidth) / 2);
    const int y = 10;
    m_searchBar->move(x, y);
}

bool MainWindow::isBuiltinPluginEnabled(const QString &pluginId) const
{
    if (!m_config)
    {
        return true;
    }
    return m_config->pluginEnabled(pluginId, true);
}

void MainWindow::setBuiltinPluginEnabled(const QString &pluginId, bool enabled)
{
    if (!m_config)
    {
        return;
    }

    m_config->setPluginEnabled(pluginId, enabled);
    m_config->save();

    if (pluginId == "builtin.waveform" && !enabled && m_waveformWidget)
    {
        m_waveformWidget->hide();
        if (m_waveMenuAction)
            m_waveMenuAction->setChecked(false);
        if (m_waveToolbarAction)
        {
            m_waveToolbarAction->setChecked(false);
            m_waveToolbarAction->setText("波形");
        }
    }
    if (pluginId == "builtin.ber" && !enabled && m_berTester)
    {
        m_berTester->hide();
    }
    if (pluginId == "builtin.report" && !enabled && m_reportPlugin)
    {
        m_reportPlugin->hide();
    }
    if (pluginId == "builtin.advanced" && !enabled && m_advancedLab)
    {
        m_advancedLab->hide();
    }
}
