#include "SerialBox/core/DataPipeline.h"
#include <QDateTime>
#include <QTimer>
#include <algorithm>

DataPipeline::DataPipeline(QObject *parent)
    : QObject(parent)
{
    // 防抖定时器：500ms 内多次设置变更只触发一次信号
    m_settingsDebounce = new QTimer(this);
    m_settingsDebounce->setSingleShot(true);
    m_settingsDebounce->setInterval(500);
    connect(m_settingsDebounce, &QTimer::timeout, this, &DataPipeline::displaySettingsChanged);
}

void DataPipeline::scheduleSettingsChanged()
{
    if (!m_settingsDebounce->isActive()) {
        m_settingsDebounce->start();
    }
    // 如果已经在运行，重置倒计时
}

void DataPipeline::addIncomingHook(const QString &name, Hook hook, int priority)
{
    m_incomingHooks.append({name, std::move(hook), priority});
    std::sort(m_incomingHooks.begin(), m_incomingHooks.end());
}

void DataPipeline::addOutgoingHook(const QString &name, Hook hook, int priority)
{
    m_outgoingHooks.append({name, std::move(hook), priority});
    std::sort(m_outgoingHooks.begin(), m_outgoingHooks.end());
}

void DataPipeline::removeHook(const QString &name)
{
    m_incomingHooks.erase(
        std::remove_if(m_incomingHooks.begin(), m_incomingHooks.end(),
                       [&](const HookEntry &e) { return e.name == name; }),
        m_incomingHooks.end());
    m_outgoingHooks.erase(
        std::remove_if(m_outgoingHooks.begin(), m_outgoingHooks.end(),
                       [&](const HookEntry &e) { return e.name == name; }),
        m_outgoingHooks.end());
}

void DataPipeline::clearHooks()
{
    m_incomingHooks.clear();
    m_outgoingHooks.clear();
}

void DataPipeline::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    emit displayModeChanged(m_displayMode);
    scheduleSettingsChanged();
}

void DataPipeline::setTimestampEnabled(bool e)
{
    if (m_timestampEnabled == e) return;
    m_timestampEnabled = e;
    scheduleSettingsChanged();
}

void DataPipeline::setAutoNewline(bool e)
{
    if (m_autoNewline == e) return;
    m_autoNewline = e;
    scheduleSettingsChanged();
}

void DataPipeline::setEchoEnabled(bool e)
{
    if (m_echoEnabled == e) return;
    m_echoEnabled = e;
    scheduleSettingsChanged();
}

void DataPipeline::applyDisplaySettings(DisplayMode mode, bool timestamp, bool autoNewline, bool echo)
{
    bool changed = false;
    if (m_displayMode != mode) {
        m_displayMode = mode;
        changed = true;
        emit displayModeChanged(mode);
    }
    if (m_timestampEnabled != timestamp) { m_timestampEnabled = timestamp; changed = true; }
    if (m_autoNewline != autoNewline) { m_autoNewline = autoNewline; changed = true; }
    if (m_echoEnabled != echo) { m_echoEnabled = echo; changed = true; }
    if (changed) {
        // 批量更新直接触发，不走防抖
        emit displaySettingsChanged();
    }
}

void DataPipeline::processIncoming(const QByteArray &raw)
{
    QByteArray data = raw;

    for (const auto &hook : m_incomingHooks) {
        data = hook.hook(data);
        if (data.isEmpty()) {
            emit dataIntercepted(raw, hook.name);
            return;
        }
    }

    emit dataReady(data, true, QDateTime::currentDateTime());
}

QByteArray DataPipeline::processOutgoing(const QByteArray &data)
{
    QByteArray out = data;

    for (const auto &hook : m_outgoingHooks) {
        out = hook.hook(out);
        if (out.isEmpty()) {
            emit dataIntercepted(data, hook.name);
            return {};
        }
    }

    return out;
}

void DataPipeline::emitOutgoingEcho(const QByteArray &data)
{
    if (!m_echoEnabled || data.isEmpty()) return;
    emit dataReady(data, false, QDateTime::currentDateTime());
}
