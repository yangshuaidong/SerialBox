/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/WorkspaceModes.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/WorkspaceModes.h"
#include <QHBoxLayout>
#include <QPushButton>

WorkspaceModes::WorkspaceModes(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void WorkspaceModes::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    struct ModeInfo
    {
        Mode mode;
        QString label;
    };
    QList<ModeInfo> modes = {
        {Mode::Basic, "基础"},
        {Mode::Expert, "专家"},
        {Mode::Automation, "自动化"},
    };

    for (const auto &info : modes)
    {
        auto *btn = new QPushButton(info.label, this);
        btn->setCheckable(true);
        btn->setProperty("mode", static_cast<int>(info.mode));
        btn->setStyleSheet(
            "QPushButton { border-radius: 6px; padding: 4px 14px; font-size: 11px; }");
        connect(btn, &QPushButton::clicked, this, [this, info]()
                { setMode(info.mode); });
        layout->addWidget(btn);
    }

    // 默认选中基础
    static_cast<QPushButton *>(layout->itemAt(0)->widget())->setChecked(true);
}

void WorkspaceModes::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;

    // 更新按钮状态
    for (auto *btn : findChildren<QPushButton *>())
    {
        btn->setChecked(btn->property("mode").toInt() == static_cast<int>(mode));
    }

    emit modeChanged(mode);
}
