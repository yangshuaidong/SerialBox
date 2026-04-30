#pragma once
#include <QObject>
#include <map>
#include <memory>
#include "ISerialPlugin.h"

class QPluginLoader;

/**
 * PluginManager — 插件动态加载与生命周期管理
 */
class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    void loadPlugins(const QString &pluginDir);
    bool loadPluginFile(const QString &filePath);
    bool unloadPlugin(const QString &name);
    void unloadAll();
    QStringList loadedPluginNames() const;
    QStringList availablePluginFiles(const QString &pluginDir) const;

    QList<ISerialPlugin *> plugins() const;
    ISerialPlugin *pluginByName(const QString &name) const;
    int pluginCount() const { return static_cast<int>(m_plugins.size()); }

    void enablePlugin(const QString &name, bool enable);
    bool isPluginEnabled(const QString &name) const;

signals:
    void pluginLoaded(const QString &name);
    void pluginUnloaded(const QString &name);
    void pluginError(const QString &name, const QString &error);

private:
    struct PluginEntry
    {
        std::unique_ptr<QPluginLoader> loader;
        ISerialPlugin *plugin = nullptr;
        QString filePath;

        PluginEntry() = default;
        PluginEntry(PluginEntry &&) = default;
        PluginEntry &operator=(PluginEntry &&) = default;
        PluginEntry(const PluginEntry &) = delete;
        PluginEntry &operator=(const PluginEntry &) = delete;
    };
    std::map<QString, PluginEntry> m_plugins;
};
