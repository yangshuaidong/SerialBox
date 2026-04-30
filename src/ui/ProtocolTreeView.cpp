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
