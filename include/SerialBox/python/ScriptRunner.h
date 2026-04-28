/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/python/ScriptRunner.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
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
