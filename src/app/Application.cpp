#include "SerialBox/app/Application.h"
#include "SerialBox/app/ConfigManager.h"
#include "SerialBox/core/SerialPortManager.h"
#include "SerialBox/core/DataPipeline.h"
#include "SerialBox/ui/MainWindow.h"
#include "SerialBox/daemon/SerialBridge.h"
#include "SerialBox/plugin/PluginManager.h"

#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>

Application::Application(QObject *parent)
    : QObject(parent)
{
}

Application::~Application() = default;

bool Application::initialize()
{
    // 1. 配置管理器
    m_config = std::make_unique<ConfigManager>(this);
    m_config->load();

    // 2. 数据管道
    m_pipeline = std::make_unique<DataPipeline>(this);

    // 3. 串口管理器
    m_serialManager = std::make_unique<SerialPortManager>(this);

    // 4. 插件管理器
    m_pluginManager = std::make_unique<PluginManager>(this);
    m_pluginManager->loadPlugins(m_config->pluginDir());

    // 5. WebSocket 守护进程（可选）
    if (m_config->bridgeEnabled())
    {
        m_bridge = std::make_unique<SerialBridge>(this);
        m_bridge->setSerialManager(m_serialManager.get());
        m_bridge->start(m_config->bridgePort());
    }

    // 5. 加载主题
    loadTheme();

    // 6. 创建主窗口
    m_mainWindow = new MainWindow();
    m_mainWindow->setManagers(m_serialManager.get(), m_config.get(), m_pipeline.get());
    m_mainWindow->show();

    // 7. 如果配置要求启动时弹串口设置
    if (m_config->showSerialDialogOnStart())
    {
        QMetaObject::invokeMethod(m_mainWindow, &MainWindow::showSerialDialog,
                                  Qt::QueuedConnection);
    }

    // 8. 连接信号
    connectSignals();

    return true;
}

void Application::loadTheme()
{
    QString theme = m_config->theme().trimmed();
    // 支持所有内置主题 + 自定义路径
    QStringList builtinThemes = {"light", "dark", "professional", "graphite", "ocean"};
    if (!builtinThemes.contains(theme) && m_config->customThemePath().isEmpty()) {
        theme = "light";
        m_config->setTheme("light");
        m_config->save();
    }
    // 自定义主题路径优先
    if (!builtinThemes.contains(theme) && !m_config->customThemePath().isEmpty()) {
        QFile custom(m_config->customThemePath());
        if (custom.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qApp->setStyleSheet(QTextStream(&custom).readAll());
            return;
        }
    }

    QFile f(QString(":/themes/%1.qss").arg(theme));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qApp->setStyleSheet(QTextStream(&f).readAll());
    }
    else
    {
        QFile fallback(":/themes/light.qss");
        if (fallback.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qApp->setStyleSheet(QTextStream(&fallback).readAll());
        }
    }
}

void Application::connectSignals()
{
    // 串口数据 → 数据管道
    connect(m_serialManager.get(), &SerialPortManager::dataReceived,
            m_pipeline.get(), &DataPipeline::processIncoming);

    // 串口发送 → 管道回显（保证所有 sendData 调用都体现在接收区）
    connect(m_serialManager.get(), &SerialPortManager::dataSent,
            m_pipeline.get(), &DataPipeline::emitOutgoingEcho);

    // 数据管道 → UI 接收区
    connect(m_pipeline.get(), &DataPipeline::dataReady,
            m_mainWindow, &MainWindow::appendReceiveData);

    // 热插拔 → UI 更新端口列表
    connect(m_serialManager.get(), &SerialPortManager::portsChanged,
            m_mainWindow, &MainWindow::refreshPortList);
}
