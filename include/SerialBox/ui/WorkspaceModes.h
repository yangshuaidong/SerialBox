/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/WorkspaceModes.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QWidget>

/**
 * WorkspaceModes — 多工作区模式切换
 *
 * 基础 / 专家 / 自动化
 */
class WorkspaceModes : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Basic, Expert, Automation };

    explicit WorkspaceModes(QWidget *parent = nullptr);

    Mode currentMode() const { return m_mode; }

signals:
    void modeChanged(Mode mode);

public slots:
    void setMode(Mode mode);

private:
    void setupUi();
    Mode m_mode = Mode::Basic;
};
