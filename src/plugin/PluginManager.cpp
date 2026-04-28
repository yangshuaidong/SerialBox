/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/plugin/PluginManager.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/plugin/PluginManager.h"
#include <QPluginLoader>
#include <QLibrary>
#include <QDir>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
}

void PluginManager::loadPlugins(const QString &pluginDir)
{
    QDir dir(pluginDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
        return;
    }

    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &fileName : entries)
    {
        QString filePath = dir.absoluteFilePath(fileName);

        // 跳过非插件文件
        if (!QLibrary::isLibrary(filePath))
            continue;
        loadPluginFile(filePath);
    }
}

bool PluginManager::loadPluginFile(const QString &filePath)
{
    if (!QLibrary::isLibrary(filePath))
    {
        return false;
    }

    for (const auto &[name, entry] : m_plugins)
    {
        if (entry.filePath == filePath)
        {
            return true;
        }
    }

    QFileInfo fi(filePath);
    const QString fileName = fi.fileName();

    auto loader = std::make_unique<QPluginLoader>(filePath);
    QObject *instance = loader->instance();

    if (!instance)
    {
        emit pluginError(fileName, loader->errorString());
        return false;
    }

    auto *plugin = qobject_cast<ISerialPlugin *>(instance);
    if (!plugin)
    {
        loader->unload();
        emit pluginError(fileName, "不是有效的 SerialBox 插件");
        return false;
    }

    if (!plugin->init())
    {
        loader->unload();
        emit pluginError(plugin->metaInfo().name, "插件初始化失败");
        return false;
    }

    plugin->onStart();

    QString name = plugin->metaInfo().name;
    if (name.isEmpty())
    {
        name = fi.baseName();
    }
    if (m_plugins.find(name) != m_plugins.end())
    {
        unloadPlugin(name);
    }

    PluginEntry entry;
    entry.loader = std::move(loader);
    entry.plugin = plugin;
    entry.filePath = filePath;
    m_plugins.emplace(name, std::move(entry));
    emit pluginLoaded(name);
    return true;
}

bool PluginManager::unloadPlugin(const QString &name)
{
    auto it = m_plugins.find(name);
    if (it == m_plugins.end())
    {
        return false;
    }

    if (it->second.plugin)
    {
        it->second.plugin->onStop();
        it->second.plugin->cleanup();
    }
    if (it->second.loader)
    {
        it->second.loader->unload();
    }
    emit pluginUnloaded(name);
    m_plugins.erase(it);
    return true;
}

void PluginManager::unloadAll()
{
    QStringList names;
    for (const auto &[name, entry] : m_plugins)
    {
        names.append(name);
    }
    for (const QString &name : names)
    {
        unloadPlugin(name);
    }
}

QStringList PluginManager::loadedPluginNames() const
{
    QStringList names;
    for (const auto &[name, entry] : m_plugins)
    {
        names.append(name);
    }
    return names;
}

QStringList PluginManager::availablePluginFiles(const QString &pluginDir) const
{
    QDir dir(pluginDir);
    if (!dir.exists())
    {
        return {};
    }
    QStringList files;
    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString &fileName : entries)
    {
        const QString fullPath = dir.absoluteFilePath(fileName);
        if (QLibrary::isLibrary(fullPath))
        {
            files.append(fullPath);
        }
    }
    return files;
}

QList<ISerialPlugin *> PluginManager::plugins() const
{
    QList<ISerialPlugin *> list;
    for (const auto &[name, entry] : m_plugins)
    {
        if (entry.plugin)
            list.append(entry.plugin);
    }
    return list;
}

ISerialPlugin *PluginManager::pluginByName(const QString &name) const
{
    auto it = m_plugins.find(name);
    return (it != m_plugins.end()) ? it->second.plugin : nullptr;
}

void PluginManager::enablePlugin(const QString &name, bool enable)
{
    auto *p = pluginByName(name);
    if (p)
    {
        p->setEnabled(enable);
        enable ? p->onStart() : p->onStop();
    }
}

bool PluginManager::isPluginEnabled(const QString &name) const
{
    auto *p = pluginByName(name);
    return p ? p->isEnabled() : false;
}
