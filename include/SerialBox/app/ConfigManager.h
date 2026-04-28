/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/app/ConfigManager.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QStringList>

/**
 * ConfigManager — 配置持久化
 *
 * 管理：
 * - 串口参数预设
 * - 窗口布局
 * - 命令库
 * - 插件空间隔离
 * - 用户偏好
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    void load();
    void save();

    // ── 串口参数 ──
    struct SerialPreset
    {
        QString portName;
        int baudRate = 115200;
        int dataBits = 8;
        int stopBits = 1;
        QString parity = "None";
        QString flowControl = "None";
        int timeoutMs = 1000;
    };
    SerialPreset serialPreset() const;
    void setSerialPreset(const SerialPreset &preset);
    QList<SerialPreset> savedPresets();
    QStringList presetNames();
    void savePreset(const QString &name, const SerialPreset &preset);
    void deletePreset(const QString &name);

    // ── 窗口布局 ──
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geo);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);
    double splitRatio() const; // 接收/发送比例，默认 0.75
    void setSplitRatio(double ratio);

    // ── 通用设置 ──
    bool showSerialDialogOnStart() const { return true; }
    QString theme() const;
    void setTheme(const QString &themeName);
    QString customThemePath() const;
    void setCustomThemePath(const QString &path);
    QString pluginDir() const;
    bool pluginEnabled(const QString &pluginId, bool defaultValue = true) const;
    void setPluginEnabled(const QString &pluginId, bool enabled);
    bool bridgeEnabled() const { return false; }
    int bridgePort() const { return 9876; }

    // ── 工作区模式 ──
    enum class WorkMode
    {
        Basic,
        Expert,
        Automation
    };
    WorkMode workMode() const;
    void setWorkMode(WorkMode mode);

    // ── 命令库 ──
    QJsonObject commandLibrary() const;
    void setCommandLibrary(const QJsonObject &lib);

    // ── 编码 ──
    QString defaultEncoding() const { return "UTF-8"; }

private:
    QSettings m_settings;
    SerialPreset m_preset;
    QJsonObject m_commands;
};
