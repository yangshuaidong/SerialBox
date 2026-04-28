/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/python/PythonEngine.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#ifdef ENABLE_PYTHON

#include "SerialBox/python/PythonEngine.h"
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QMutexLocker>

PythonEngine::PythonEngine(QObject *parent)
    : QObject(parent)
{
}

PythonEngine::~PythonEngine()
{
    shutdown();
}

bool PythonEngine::initialize()
{
    try {
        py::initialize_interpreter();
        m_initialized = true;

        // 注册核心 API
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")("scripts");

        // 注册 serialbox 模块 — 绑定核心 API
        py::module_ sb = py::module_::create_module("serialbox");

        sb.def("log", [this](const std::string &msg) {
            emit scriptOutput(QString::fromStdString(msg));
        });

        sb.def("sleep_ms", [](int ms) {
            QThread::msleep(static_cast<unsigned long>(ms));
        });

        // 回调注册
        sb.def("on_data", [this](py::function cb) {
            QMutexLocker lock(&m_callbackMutex);
            m_dataCallbacks.append(cb);
        });

        // 全局可用
        py::module_::import("sys").attr("modules")["serialbox"] = sb;

        return true;
    } catch (const py::error_already_set &e) {
        emit scriptError(QString("Python 初始化失败: %1").arg(e.what()));
        return false;
    }
}

void PythonEngine::shutdown()
{
    if (m_initialized) {
        {
            QMutexLocker lock(&m_callbackMutex);
            m_dataCallbacks.clear();
        }
        py::finalize_interpreter();
        m_initialized = false;
    }
}

void PythonEngine::executeScript(const QString &script)
{
    if (!m_initialized) {
        emit scriptError("Python 引擎未初始化");
        return;
    }

    // 独立线程 + 超时保护
    QThread *worker = QThread::create([this, script]() {
        try {
            py::exec(script.toStdString());
            emit scriptFinished(0);
        } catch (const py::error_already_set &e) {
            emit scriptError(QString::fromStdString(e.what()));
            emit scriptFinished(1);
        }
    });

    connect(worker, &QThread::finished, worker, &QThread::deleteLater);

    // 超时保护
    QTimer::singleShot(m_timeoutMs, this, [worker]() {
        if (worker->isRunning()) {
            worker->terminate();
        }
    });

    worker->start();
}

void PythonEngine::executeFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit scriptError(QString("无法打开脚本: %1").arg(filePath));
        return;
    }
    executeScript(QString::fromUtf8(file.readAll()));
}

void PythonEngine::setExecutionTimeoutMs(int ms)
{
    m_timeoutMs = qMax(1000, ms);
}

void PythonEngine::registerCallback(const QString &name, Callback cb)
{
    QMutexLocker lock(&m_callbackMutex);
    m_cppCallbacks.insert(name, std::move(cb));

    // 同步暴露给 Python
    if (m_initialized) {
        try {
            py::module_ sb = py::module_::import("serialbox");
            std::string cppName = name.toStdString();
            sb.attr(cppName.c_str()) = py::cpp_function(
                [this, name](const std::string &arg) {
                    QMutexLocker lock(&m_callbackMutex);
                    auto it = m_cppCallbacks.constFind(name);
                    if (it != m_cppCallbacks.constEnd()) {
                        it.value()(QString::fromStdString(arg));
                    }
                });
        } catch (const py::error_already_set &e) {
            emit scriptError(QString("注册回调失败 %1: %2").arg(name, e.what()));
        }
    }
}

void PythonEngine::feedDataToCallbacks(const QString &data)
{
    QMutexLocker lock(&m_callbackMutex);
    for (const auto &cb : m_dataCallbacks) {
        try {
            cb(data.toStdString());
        } catch (...) {}
    }
}

#else

// 无 Python 支持时的空实现
#include "SerialBox/python/PythonEngine.h"

PythonEngine::PythonEngine(QObject *parent) : QObject(parent) {}
PythonEngine::~PythonEngine() = default;
bool PythonEngine::initialize() { return false; }
void PythonEngine::shutdown() {}
void PythonEngine::executeScript(const QString &) { emit scriptError("Python 引擎未编译"); }
void PythonEngine::executeFile(const QString &) { emit scriptError("Python 引擎未编译"); }
void PythonEngine::setExecutionTimeoutMs(int ms) { m_timeoutMs = ms; }
void PythonEngine::registerCallback(const QString &, Callback) {}
void PythonEngine::feedDataToCallbacks(const QString &) {}

#endif
