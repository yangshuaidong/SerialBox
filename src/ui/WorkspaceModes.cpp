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
