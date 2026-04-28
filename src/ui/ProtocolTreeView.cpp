/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/ProtocolTreeView.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/ProtocolTreeView.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QLabel>

ProtocolTreeView::ProtocolTreeView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ProtocolTreeView::setupUi()
{
    setWindowTitle("协议解析");
    setMinimumSize(400, 300);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *tree = new QTreeWidget(this);
    tree->setHeaderLabels({"字段", "值", "类型"});
    tree->setObjectName("tree");
    layout->addWidget(tree);
}

void ProtocolTreeView::setProtocolData(const QVariantMap &data, const QString &protocolName)
{
    auto *tree = findChild<QTreeWidget *>("tree");
    if (!tree)
        return;
    tree->clear();

    auto *root = new QTreeWidgetItem(tree, {protocolName, "", "协议"});
    root->setExpanded(true);

    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
    {
        auto *item = new QTreeWidgetItem(root, {it.key(), it.value().toString(), it.value().typeName()});
    }
}
