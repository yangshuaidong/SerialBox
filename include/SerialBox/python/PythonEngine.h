#pragma once
#include <QObject>
#include <QThread>
#include <QString>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <functional>

#ifdef ENABLE_PYTHON
#include <pybind11/embed.h>
#include <pybind11/functional.h>
namespace py = pybind11;
#endif

/**
 * PythonEngine — 嵌入式 Python 环境 (pybind11)
 *
 * 安全沙箱与独立线程调度
 * 防脚本卡死 UI
 *
 * Python 中可通过 import serialbox 使用：
 *   serialbox.log("消息")         — 输出到接收区
 *   serialbox.sleep_ms(100)       — 毫秒休眠
 *   serialbox.on_data(callback)   — 注册数据接收回调
 *   serialbox.my_callback(arg)    — C++ 注册的回调
 */
class PythonEngine : public QObject
{
    Q_OBJECT

public:
    explicit PythonEngine(QObject *parent = nullptr);
    ~PythonEngine() override;

    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── 执行 ──
    void executeScript(const QString &script);
    void executeFile(const QString &filePath);

    // ── 超时保护 ──
    void setExecutionTimeoutMs(int ms);
    int executionTimeoutMs() const { return m_timeoutMs; }

    // ── API 绑定 ──
    using Callback = std::function<void(const QString &)>;
    void registerCallback(const QString &name, Callback cb);

    // ── 数据推送 ──
    void feedDataToCallbacks(const QString &data);

signals:
    void scriptOutput(const QString &output);
    void scriptError(const QString &error);
    void scriptFinished(int exitCode);

private:
    int m_timeoutMs = 30000;
    bool m_initialized = false;
    QMutex m_callbackMutex;
    QMap<QString, Callback> m_cppCallbacks;

#ifdef ENABLE_PYTHON
    QVector<py::function> m_dataCallbacks;
#endif
};
