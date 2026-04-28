/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/CommandSidebar.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/CommandSidebar.h"
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QLineEdit>
// 中文注释
//  CommandSidebar.cpp
CommandSidebar::CommandSidebar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}
// 设置命令库
void CommandSidebar::setupUi()
{
    setFixedWidth(280);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // 标题栏
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    auto *title = new QLabel("命令库", header);
    title->setStyleSheet("font-size: 13px; font-weight: 600; padding: 8px 14px;");
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    layout->addWidget(header);

    // 命令树
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);

    // 常用 AT 指令
    auto *atGroup = new QTreeWidgetItem(m_tree, {"常用 AT 指令"});
    struct Cmd
    {
        QString key, label, data;
    };
    QList<Cmd> atCmds = {
        {"F1", "查询版本", "AT+VERSION?"},
        {"F2", "查询传感器", "AT+SENSOR?"},
        {"F3", "LED开", "AT+LED=ON"},
        {"F4", "LED关", "AT+LED=OFF"},
        {"F5", "重启", "AT+REBOOT"},
    };
    for (const auto &cmd : atCmds)
    {
        auto *item = new QTreeWidgetItem(atGroup, {QString("%1  %2").arg(cmd.key, cmd.label)});
        item->setData(0, Qt::UserRole, cmd.data);
        item->setData(0, Qt::UserRole + 1, false);
    }

    // Modbus 查询
    auto *modbusGroup = new QTreeWidgetItem(m_tree, {"Modbus 查询"});
    QList<Cmd> modbusCmds = {
        {"F6", "读保持寄存器", "01 03 00 00 00 01 84 0A"},
        {"F7", "读输入寄存器", "01 04 00 00 00 02 71 CB"},
        {"F8", "写单寄存器", "01 06 00 01 00 03 98 0B"},
    };
    for (const auto &cmd : modbusCmds)
    {
        auto *item = new QTreeWidgetItem(modbusGroup, {QString("%1  %2").arg(cmd.key, cmd.label)});
        item->setData(0, Qt::UserRole, cmd.data);
        item->setData(0, Qt::UserRole + 1, false);
    }

    // 自定义
    m_customGroup = new QTreeWidgetItem(m_tree, {"自定义"});
    auto *item1 = new QTreeWidgetItem(m_customGroup, {"F9  心跳包"});
    item1->setData(0, Qt::UserRole, "PING_${timestamp}");
    item1->setData(0, Qt::UserRole + 1, true);
    auto *item2 = new QTreeWidgetItem(m_customGroup, {"F10 读全部"});
    item2->setData(0, Qt::UserRole, "01 03 00 00 00 0A C5 CD");
    item2->setData(0, Qt::UserRole + 1, true);

    m_tree->expandAll();
    layout->addWidget(m_tree, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(8, 4, 8, 8);
    m_addBtn = new QPushButton("新增", this);
    m_editBtn = new QPushButton("修改", this);
    m_removeBtn = new QPushButton("删除", this);
    m_addBtn->setStyleSheet("QPushButton { padding: 4px 10px; font-size: 12px; }");
    m_editBtn->setStyleSheet("QPushButton { padding: 4px 10px; font-size: 12px; }");
    m_removeBtn->setStyleSheet("QPushButton { padding: 4px 10px; font-size: 12px; }");
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_addBtn, &QPushButton::clicked, this, &CommandSidebar::addCustomCommand);
    connect(m_editBtn, &QPushButton::clicked, this, &CommandSidebar::editCurrentCommand);
    connect(m_removeBtn, &QPushButton::clicked, this, &CommandSidebar::removeCurrentCommand);

    // 双击 → 发送命令
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
            {
        QString data = item->data(0, Qt::UserRole).toString();
        if (!data.isEmpty()) {
            emit commandSelected(data);
        } });
}
// 设置命令库
void CommandSidebar::setCommandLibrary(const QJsonObject &library)
{
    if (!m_tree || !m_customGroup)
        return;

    qDeleteAll(m_customGroup->takeChildren());
    const QJsonArray arr = library.value("custom").toArray();
    for (const QJsonValue &v : arr)
    {
        const QJsonObject obj = v.toObject();
        const QString name = obj.value("name").toString();
        const QString cmd = obj.value("cmd").toString();
        if (name.isEmpty() || cmd.isEmpty())
        {
            continue;
        }
        auto *item = new QTreeWidgetItem(m_customGroup, {name});
        item->setData(0, Qt::UserRole, cmd);
        item->setData(0, Qt::UserRole + 1, true);
    }

    // 若配置为空则保留默认示例，避免空白
    if (m_customGroup->childCount() == 0)
    {
        auto *item = new QTreeWidgetItem(m_customGroup, {"心跳包"});
        item->setData(0, Qt::UserRole, "PING_${timestamp}");
        item->setData(0, Qt::UserRole + 1, true);
    }
    m_tree->expandItem(m_customGroup);
}
// 获取命令库
QJsonObject CommandSidebar::commandLibrary() const
{
    QJsonObject out;
    QJsonArray custom;
    if (m_customGroup)
    {
        for (int i = 0; i < m_customGroup->childCount(); ++i)
        {
            QTreeWidgetItem *item = m_customGroup->child(i);
            QJsonObject obj;
            obj.insert("name", item->text(0));
            obj.insert("cmd", item->data(0, Qt::UserRole).toString());
            custom.append(obj);
        }
    }
    out.insert("custom", custom);
    return out;
}

void CommandSidebar::addCustomCommand()
{
    if (!m_customGroup)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, "新增命令", "命令名称", QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    const QString cmd = QInputDialog::getMultiLineText(this, "新增命令", "命令内容", {}, &ok).trimmed();
    if (!ok || cmd.isEmpty())
        return;

    auto *item = new QTreeWidgetItem(m_customGroup, {name});
    item->setData(0, Qt::UserRole, cmd);
    item->setData(0, Qt::UserRole + 1, true);
    m_tree->setCurrentItem(item);
    emit commandLibraryChanged(commandLibrary());
}

void CommandSidebar::editCurrentCommand()
{
    QTreeWidgetItem *item = m_tree ? m_tree->currentItem() : nullptr;
    if (!item || item->data(0, Qt::UserRole + 1).toBool() != true)
    {
        QMessageBox::information(this, "修改命令", "请选择一条“自定义”命令后再修改。");
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, "修改命令", "命令名称", QLineEdit::Normal, item->text(0), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    const QString currentCmd = item->data(0, Qt::UserRole).toString();
    const QString cmd = QInputDialog::getMultiLineText(this, "修改命令", "命令内容", currentCmd, &ok).trimmed();
    if (!ok || cmd.isEmpty())
        return;

    item->setText(0, name);
    item->setData(0, Qt::UserRole, cmd);
    emit commandLibraryChanged(commandLibrary());
}

void CommandSidebar::removeCurrentCommand()
{
    QTreeWidgetItem *item = m_tree ? m_tree->currentItem() : nullptr;
    if (!item || item->data(0, Qt::UserRole + 1).toBool() != true || !m_customGroup)
    {
        QMessageBox::information(this, "删除命令", "请选择一条“自定义”命令后再删除。");
        return;
    }
    delete item;
    emit commandLibraryChanged(commandLibrary());
}

QTreeWidgetItem *CommandSidebar::findGroupItem(const QString &name) const
{
    if (!m_tree)
        return nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        if (item && item->text(0) == name)
        {
            return item;
        }
    }
    return nullptr;
}
