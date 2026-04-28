/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/ProtocolTreeView.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QWidget>

/**
 * ProtocolTreeView — 协议解析树 (JSON/Modbus 折叠展示)
 */
class ProtocolTreeView : public QWidget
{
    Q_OBJECT

public:
    explicit ProtocolTreeView(QWidget *parent = nullptr);

public slots:
    void setProtocolData(const QVariantMap &data, const QString &protocolName);

private:
    void setupUi();
};
