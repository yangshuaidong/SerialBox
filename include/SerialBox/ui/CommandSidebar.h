/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/CommandSidebar.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QWidget>
#include <QJsonObject>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

/**
 * CommandSidebar — 命令库侧边栏
 *
 * 分组/快捷键/变量替换
 */
class CommandSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit CommandSidebar(QWidget *parent = nullptr);

    void setCommandLibrary(const QJsonObject &library);
    QJsonObject commandLibrary() const;

signals:
    void commandSelected(const QString &command);
    void commandLibraryChanged(const QJsonObject &library);

private:
    void setupUi();
    void addCustomCommand();
    void editCurrentCommand();
    void removeCurrentCommand();
    QTreeWidgetItem *findGroupItem(const QString &name) const;

    QTreeWidget *m_tree = nullptr;
    QTreeWidgetItem *m_customGroup = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
};
