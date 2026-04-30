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
