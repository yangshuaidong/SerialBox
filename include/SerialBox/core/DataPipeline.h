/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/DataPipeline.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QByteArray>
#include <functional>

/**
 * DataPipeline — 数据拦截与预处理钩子
 *
 * 数据流转基座：
 *   串口原始数据 → [预处理钩子链] → UI/协议解析/插件
 *
 * 钩子示例：编码检测、HEX转换、协议帧提取、过滤
 */
class DataPipeline : public QObject
{
    Q_OBJECT

public:
    explicit DataPipeline(QObject *parent = nullptr);

    // ── 钩子定义 ──
    using Hook = std::function<QByteArray(const QByteArray &data)>;
    void addIncomingHook(const QString &name, Hook hook, int priority = 0);
    void addOutgoingHook(const QString &name, Hook hook, int priority = 0);
    void removeHook(const QString &name);
    void clearHooks();

    // ── 数据流 ──
    enum class DisplayMode
    {
        Text,
        Hex
    };
    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    void setTimestampEnabled(bool enable);
    bool timestampEnabled() const { return m_timestampEnabled; }

    void setAutoNewline(bool enable);
    bool autoNewline() const { return m_autoNewline; }

    // ── 发送回显 ──
    void setEchoEnabled(bool enable);
    bool echoEnabled() const { return m_echoEnabled; }

public slots:
    void processIncoming(const QByteArray &raw);
    QByteArray processOutgoing(const QByteArray &data);
    void emitOutgoingEcho(const QByteArray &data);

signals:
    void dataReady(const QByteArray &processed, bool isReceive, const QDateTime &timestamp);
    void displayModeChanged(DisplayMode mode);

private:
    struct HookEntry
    {
        QString name;
        Hook hook;
        int priority;
        bool operator<(const HookEntry &other) const { return priority > other.priority; }
    };

    QList<HookEntry> m_incomingHooks;
    QList<HookEntry> m_outgoingHooks;
    DisplayMode m_displayMode = DisplayMode::Text;
    bool m_timestampEnabled = true;
    bool m_autoNewline = true;
    bool m_echoEnabled = true;
};
