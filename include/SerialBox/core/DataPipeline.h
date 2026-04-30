#pragma once
#include <QObject>
#include <QByteArray>
#include <functional>

class QTimer;

/**
 * DataPipeline — 数据拦截与预处理钩子
 *
 * 数据流转基座：
 *   串口原始数据 → [预处理钩子链] → UI/协议解析/插件
 *
 * 钩子示例：编码检测、HEX转换、协议帧提取、过滤
 *
 * 配置变更通过 displaySettingsChanged 信号通知 ConfigManager 持久化。
 * 信号带 500ms 防抖，避免连续属性变更触发多次磁盘写入。
 * 钩子拦截数据时发出 dataIntercepted 信号，避免静默丢弃。
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
    enum class DisplayMode { Text, Hex };
    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    void setTimestampEnabled(bool enable);
    bool timestampEnabled() const { return m_timestampEnabled; }

    void setAutoNewline(bool enable);
    bool autoNewline() const { return m_autoNewline; }

    void setEchoEnabled(bool enable);
    bool echoEnabled() const { return m_echoEnabled; }

    /**
     * 批量更新显示设置（只触发一次 displaySettingsChanged）
     */
    void applyDisplaySettings(DisplayMode mode, bool timestamp, bool autoNewline, bool echo);

public slots:
    void processIncoming(const QByteArray &raw);
    QByteArray processOutgoing(const QByteArray &data);
    void emitOutgoingEcho(const QByteArray &data);

signals:
    void dataReady(const QByteArray &processed, bool isReceive, const QDateTime &timestamp);
    void displayModeChanged(DisplayMode mode);

    // 钩子拦截数据时发出（避免静默丢弃）
    void dataIntercepted(const QByteArray &intercepted, const QString &hookName);

    // 任一显示配置变更时发出（带 500ms 防抖），供 ConfigManager 持久化
    void displaySettingsChanged();

private:
    void scheduleSettingsChanged();

    struct HookEntry {
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

    QTimer *m_settingsDebounce = nullptr;
};
