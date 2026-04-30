#pragma once
#include <QObject>
#include <QThread>

class PythonEngine;

/**
 * ScriptRunner — Python 脚本独立线程调度器
 *
 * 防脚本卡死 UI：
 * - 独立 QThread 运行脚本
 * - 超时自动终止
 * - 模块白名单控制
 */
class ScriptRunner : public QObject
{
    Q_OBJECT

public:
    explicit ScriptRunner(PythonEngine *engine, QObject *parent = nullptr);

    void run(const QString &script, int timeoutMs = 30000);
    void stop();
    bool isRunning() const { return m_running; }

    // ── 模块白名单 ──
    void setAllowedModules(const QStringList &modules);
    QStringList allowedModules() const { return m_allowedModules; }

signals:
    void started();
    void finished(int exitCode);
    void output(const QString &text);
    void error(const QString &text);
    void timeout();

private:
    PythonEngine *m_engine = nullptr;
    bool m_running = false;
    bool m_shouldStop = false;
    QStringList m_allowedModules = {"math", "json", "time", "struct", "collections", "serialbox"};
};
