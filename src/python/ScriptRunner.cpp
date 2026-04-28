/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/python/ScriptRunner.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#ifdef ENABLE_PYTHON

#include "SerialBox/python/ScriptRunner.h"
#include "SerialBox/python/PythonEngine.h"
#include <QTimer>

ScriptRunner::ScriptRunner(PythonEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

void ScriptRunner::run(const QString &script, int timeoutMs)
{
    if (m_running) return;
    m_running = true;
    m_shouldStop = false;
    emit started();

    // 模块白名单沙箱脚本
    QString sandboxed = QString(
        "import sys\n"
        "_allowed = %1\n"
        "_original_import = __builtins__.__import__\n"
        "def _safe_import(name, *args, **kwargs):\n"
        "    if name.split('.')[0] not in _allowed:\n"
        "        raise ImportError(f'Module {name} is not allowed')\n"
        "    return _original_import(name, *args, **kwargs)\n"
        "__builtins__.__import__ = _safe_import\n\n"
    ).arg("['" + m_allowedModules.join("','") + "']");

    // 注入停止标志检查
    sandboxed += QString(
        "import serialbox as _sb\n"
        "_sb._should_stop = lambda: %1\n"
        "\n"
    ).arg(m_shouldStop ? "True" : "False");

    sandboxed += script;

    // 连接信号
    auto outputConn = connect(m_engine, &PythonEngine::scriptOutput, this, &ScriptRunner::output);
    auto errorConn = connect(m_engine, &PythonEngine::scriptError, this, &ScriptRunner::error);
    auto finishedConn = connect(m_engine, &PythonEngine::scriptFinished, this, [this, outputConn, errorConn, finishedConn](int code) {
        disconnect(outputConn);
        disconnect(errorConn);
        disconnect(finishedConn);
        m_running = false;
        emit finished(code);
    });

    m_engine->setExecutionTimeoutMs(timeoutMs);
    m_engine->executeScript(sandboxed);
}

void ScriptRunner::stop()
{
    m_shouldStop = true;
    m_running = false;
    emit finished(-1);
}

void ScriptRunner::setAllowedModules(const QStringList &modules)
{
    m_allowedModules = modules;
}

#else

#include "SerialBox/python/ScriptRunner.h"
#include "SerialBox/python/PythonEngine.h"

ScriptRunner::ScriptRunner(PythonEngine *, QObject *parent) : QObject(parent) {}
void ScriptRunner::run(const QString &, int) { emit error("Python 引擎未编译"); }
void ScriptRunner::stop() { m_running = false; }
void ScriptRunner::setAllowedModules(const QStringList &modules) { m_allowedModules = modules; }

#endif
