/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/plugin/ISerialPlugin.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QString>

/**
 * ISerialPlugin — 插件接口定义
 *
 * 生命周期: init() → onStart() → [功能] → onStop() → cleanup()
 *
 * 插件可以：
 * - 拦截收发数据 (DataPipeline 钩子)
 * - 注册协议解析器
 * - 注册 UI 面板
 * - 提供 CLI 命令
 */
class ISerialPlugin : public QObject
{
    Q_OBJECT

public:
    struct MetaInfo {
        QString name;
        QString version;
        QString author;
        QString description;
    };

    explicit ISerialPlugin(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ISerialPlugin() = default;

    virtual MetaInfo metaInfo() const = 0;

    // ── 生命周期 ──
    virtual bool init() { return true; }
    virtual void onStart() {}
    virtual void onStop() {}
    virtual void cleanup() {}

    // ── 能力声明 ──
    virtual bool hasDataHook() const { return false; }
    virtual bool hasProtocolParser() const { return false; }
    virtual bool hasUiPanel() const { return false; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = true;
};

#define SerialPlugin_IID "com.serialbox.ISerialPlugin/1.0"
Q_DECLARE_INTERFACE(ISerialPlugin, SerialPlugin_IID)
