#include "SerialBox/app/ConfigManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent), m_settings("SerialBox", "SerialBox")
{
}

void ConfigManager::load()
{
    m_preset.portName = m_settings.value("serial/port", "").toString();
    m_preset.baudRate = m_settings.value("serial/baud", 115200).toInt();
    m_preset.dataBits = m_settings.value("serial/dataBits", 8).toInt();
    m_preset.stopBits = m_settings.value("serial/stopBits", 1).toInt();
    m_preset.parity = m_settings.value("serial/parity", "None").toString();
    m_preset.flowControl = m_settings.value("serial/flow", "None").toString();
    m_preset.timeoutMs = m_settings.value("serial/timeout", 1000).toInt();

    // 显示设置（首次运行时使用默认值）
    // displaySettings() 是从 m_settings 直接读取的，此处无需额外操作
    // 但显式调用一次确保默认值写入磁盘
    auto ds = displaySettings();
    Q_UNUSED(ds);

    auto doc = QJsonDocument::fromJson(m_settings.value("commands/library").toByteArray());
    m_commands = doc.object();
}

void ConfigManager::save()
{
    m_settings.setValue("serial/port", m_preset.portName);
    m_settings.setValue("serial/baud", m_preset.baudRate);
    m_settings.setValue("serial/dataBits", m_preset.dataBits);
    m_settings.setValue("serial/stopBits", m_preset.stopBits);
    m_settings.setValue("serial/parity", m_preset.parity);
    m_settings.setValue("serial/flow", m_preset.flowControl);
    m_settings.setValue("serial/timeout", m_preset.timeoutMs);
    m_settings.setValue("commands/library", QJsonDocument(m_commands).toJson());
    m_settings.sync();
}

ConfigManager::SerialPreset ConfigManager::serialPreset() const { return m_preset; }
void ConfigManager::setSerialPreset(const SerialPreset &p) { m_preset = p; }

QByteArray ConfigManager::windowGeometry() const
{
    return m_settings.value("ui/geometry").toByteArray();
}
void ConfigManager::setWindowGeometry(const QByteArray &geo)
{
    m_settings.setValue("ui/geometry", geo);
}

QByteArray ConfigManager::windowState() const
{
    return m_settings.value("ui/state").toByteArray();
}
void ConfigManager::setWindowState(const QByteArray &state)
{
    m_settings.setValue("ui/state", state);
}

double ConfigManager::splitRatio() const
{
    return m_settings.value("ui/splitRatio", 0.75).toDouble();
}
void ConfigManager::setSplitRatio(double r)
{
    m_settings.setValue("ui/splitRatio", r);
}

// ── 显示设置（与 DataPipeline 对齐）──
ConfigManager::DisplaySettings ConfigManager::displaySettings() const
{
    DisplaySettings s;
    s.displayMode = m_settings.value("display/mode", 0).toInt();
    s.timestampEnabled = m_settings.value("display/timestamp", true).toBool();
    s.autoNewline = m_settings.value("display/autoNewline", true).toBool();
    s.echoEnabled = m_settings.value("display/echo", true).toBool();
    return s;
}

void ConfigManager::setDisplaySettings(const DisplaySettings &s)
{
    m_settings.setValue("display/mode", s.displayMode);
    m_settings.setValue("display/timestamp", s.timestampEnabled);
    m_settings.setValue("display/autoNewline", s.autoNewline);
    m_settings.setValue("display/echo", s.echoEnabled);
}

QString ConfigManager::theme() const
{
    return m_settings.value("ui/theme", "dark").toString();
}

void ConfigManager::setTheme(const QString &themeName)
{
    m_settings.setValue("ui/theme", themeName);
}

QString ConfigManager::customThemePath() const
{
    return m_settings.value("ui/customThemePath", "").toString();
}

void ConfigManager::setCustomThemePath(const QString &path)
{
    m_settings.setValue("ui/customThemePath", path);
}

QString ConfigManager::pluginDir() const
{
    return m_settings.value("plugins/dir",
                            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins")
        .toString();
}

bool ConfigManager::pluginEnabled(const QString &pluginId, bool defaultValue) const
{
    return m_settings.value(QString("plugins/enabled/%1").arg(pluginId), defaultValue).toBool();
}

void ConfigManager::setPluginEnabled(const QString &pluginId, bool enabled)
{
    m_settings.setValue(QString("plugins/enabled/%1").arg(pluginId), enabled);
}

ConfigManager::WorkMode ConfigManager::workMode() const
{
    return static_cast<WorkMode>(m_settings.value("ui/workMode", 0).toInt());
}
void ConfigManager::setWorkMode(WorkMode m)
{
    m_settings.setValue("ui/workMode", static_cast<int>(m));
}

QJsonObject ConfigManager::commandLibrary() const { return m_commands; }
void ConfigManager::setCommandLibrary(const QJsonObject &lib) { m_commands = lib; }

QList<ConfigManager::SerialPreset> ConfigManager::savedPresets()
{
    QList<SerialPreset> list;
    int count = m_settings.beginReadArray("presets");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        SerialPreset p;
        p.portName = m_settings.value("port").toString();
        p.baudRate = m_settings.value("baud").toInt();
        p.dataBits = m_settings.value("dataBits").toInt();
        p.stopBits = m_settings.value("stopBits").toInt();
        p.parity = m_settings.value("parity").toString();
        p.flowControl = m_settings.value("flow").toString();
        p.timeoutMs = m_settings.value("timeout").toInt();
        list.append(p);
    }
    m_settings.endArray();
    return list;
}

QStringList ConfigManager::presetNames()
{
    QStringList names;
    int count = m_settings.beginReadArray("presets");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        names.append(m_settings.value("name").toString());
    }
    m_settings.endArray();
    return names;
}

void ConfigManager::savePreset(const QString &name, const SerialPreset &p)
{
    struct NamedPreset { QString name; SerialPreset preset; };
    QList<NamedPreset> items;
    const int count = m_settings.beginReadArray("presets");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        NamedPreset np;
        np.name = m_settings.value("name").toString();
        np.preset.portName = m_settings.value("port").toString();
        np.preset.baudRate = m_settings.value("baud", 115200).toInt();
        np.preset.dataBits = m_settings.value("dataBits", 8).toInt();
        np.preset.stopBits = m_settings.value("stopBits", 1).toInt();
        np.preset.parity = m_settings.value("parity", "None").toString();
        np.preset.flowControl = m_settings.value("flow", "None").toString();
        np.preset.timeoutMs = m_settings.value("timeout", 1000).toInt();
        items.append(np);
    }
    m_settings.endArray();

    bool replaced = false;
    for (auto &it : items) {
        if (it.name.compare(name, Qt::CaseInsensitive) == 0) {
            it.preset = p;
            replaced = true;
            break;
        }
    }
    if (!replaced) items.append({name, p});

    m_settings.beginWriteArray("presets", items.size());
    for (int i = 0; i < items.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("name", items[i].name);
        m_settings.setValue("port", items[i].preset.portName);
        m_settings.setValue("baud", items[i].preset.baudRate);
        m_settings.setValue("dataBits", items[i].preset.dataBits);
        m_settings.setValue("stopBits", items[i].preset.stopBits);
        m_settings.setValue("parity", items[i].preset.parity);
        m_settings.setValue("flow", items[i].preset.flowControl);
        m_settings.setValue("timeout", items[i].preset.timeoutMs);
    }
    m_settings.endArray();
}

void ConfigManager::deletePreset(const QString &name)
{
    struct NamedPreset { QString name; SerialPreset preset; };
    QList<NamedPreset> items;
    const int count = m_settings.beginReadArray("presets");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        NamedPreset np;
        np.name = m_settings.value("name").toString();
        np.preset.portName = m_settings.value("port").toString();
        np.preset.baudRate = m_settings.value("baud", 115200).toInt();
        np.preset.dataBits = m_settings.value("dataBits", 8).toInt();
        np.preset.stopBits = m_settings.value("stopBits", 1).toInt();
        np.preset.parity = m_settings.value("parity", "None").toString();
        np.preset.flowControl = m_settings.value("flow", "None").toString();
        np.preset.timeoutMs = m_settings.value("timeout", 1000).toInt();
        if (np.name.compare(name, Qt::CaseInsensitive) != 0)
            items.append(np);
    }
    m_settings.endArray();

    m_settings.beginWriteArray("presets", items.size());
    for (int i = 0; i < items.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("name", items[i].name);
        m_settings.setValue("port", items[i].preset.portName);
        m_settings.setValue("baud", items[i].preset.baudRate);
        m_settings.setValue("dataBits", items[i].preset.dataBits);
        m_settings.setValue("stopBits", items[i].preset.stopBits);
        m_settings.setValue("parity", items[i].preset.parity);
        m_settings.setValue("flow", items[i].preset.flowControl);
        m_settings.setValue("timeout", items[i].preset.timeoutMs);
    }
    m_settings.endArray();
}
