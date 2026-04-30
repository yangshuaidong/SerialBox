#include "SerialBox/core/DataPipeline.h"
#include <QDateTime>
#include <algorithm>

DataPipeline::DataPipeline(QObject *parent)
    : QObject(parent)
{
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
    emit displaySettingsChanged();
}

void DataPipeline::setTimestampEnabled(bool e)
{
    if (m_timestampEnabled == e) return;
    m_timestampEnabled = e;
    emit displaySettingsChanged();
}

void DataPipeline::setAutoNewline(bool e)
{
    if (m_autoNewline == e) return;
    m_autoNewline = e;
    emit displaySettingsChanged();
}

void DataPipeline::setEchoEnabled(bool e)
{
    if (m_echoEnabled == e) return;
    m_echoEnabled = e;
    emit displaySettingsChanged();
}

void DataPipeline::processIncoming(const QByteArray &raw)
{
    QByteArray data = raw;

    // 执行钩子链
    for (const auto &hook : m_incomingHooks) {
        data = hook.hook(data);
        if (data.isEmpty()) {
            // 钩子拦截了数据 — 发出信号而非静默丢弃
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
