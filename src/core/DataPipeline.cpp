/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/core/DataPipeline.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
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
                       [&](const HookEntry &e)
                       { return e.name == name; }),
        m_incomingHooks.end());
    m_outgoingHooks.erase(
        std::remove_if(m_outgoingHooks.begin(), m_outgoingHooks.end(),
                       [&](const HookEntry &e)
                       { return e.name == name; }),
        m_outgoingHooks.end());
}

void DataPipeline::clearHooks()
{
    m_incomingHooks.clear();
    m_outgoingHooks.clear();
}

void DataPipeline::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode == mode)
    {
        return;
    }
    m_displayMode = mode;
    emit displayModeChanged(m_displayMode);
}
void DataPipeline::setTimestampEnabled(bool e) { m_timestampEnabled = e; }
void DataPipeline::setAutoNewline(bool e) { m_autoNewline = e; }
void DataPipeline::setEchoEnabled(bool e) { m_echoEnabled = e; }

void DataPipeline::processIncoming(const QByteArray &raw)
{
    QByteArray data = raw;

    // 执行钩子链
    for (const auto &hook : m_incomingHooks)
    {
        data = hook.hook(data);
        if (data.isEmpty())
            return; // 钩子拦截了数据
    }

    emit dataReady(data, true, QDateTime::currentDateTime());
}

QByteArray DataPipeline::processOutgoing(const QByteArray &data)
{
    QByteArray out = data;

    for (const auto &hook : m_outgoingHooks)
    {
        out = hook.hook(out);
        if (out.isEmpty())
            return {};
    }

    return out;
}

void DataPipeline::emitOutgoingEcho(const QByteArray &data)
{
    if (!m_echoEnabled || data.isEmpty())
    {
        return;
    }
    emit dataReady(data, false, QDateTime::currentDateTime());
}
