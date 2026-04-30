#pragma once
#include <QObject>
#include <memory>

class MainWindow;
class SerialPortManager;
class ConfigManager;
class DataPipeline;
class SerialBridge;
class PluginManager;

/**
 * Application — 全局应用生命周期管理
 *
 * 职责：
 * - 初始化所有子系统（配置、串口、管道、插件、守护进程）
 * - 管理主窗口
 * - 处理优雅退出
 */
class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    bool initialize();

    MainWindow *mainWindow() const { return m_mainWindow; }
    SerialPortManager *serialManager() const { return m_serialManager.get(); }
    ConfigManager *config() const { return m_config.get(); }
    DataPipeline *pipeline() const { return m_pipeline.get(); }
    PluginManager *pluginManager() const { return m_pluginManager.get(); }

private:
    void loadTheme();
    void connectSignals();

    MainWindow *m_mainWindow = nullptr;
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<SerialPortManager> m_serialManager;
    std::unique_ptr<DataPipeline> m_pipeline;
    std::unique_ptr<SerialBridge> m_bridge;
    std::unique_ptr<PluginManager> m_pluginManager;
};
