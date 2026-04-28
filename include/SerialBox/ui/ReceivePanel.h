/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/ReceivePanel.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QDateTime>
#include <QJsonArray>
#include <QSet>

class DataPipeline;

/**
 * ReceivePanel — 接收区 (虚拟列表)
 *
 * 支持 10万+ 行流畅滚动
 * - 文本/HEX 双模式
 * - 时间戳显示
 * - 搜索高亮
 * - 协议解析标签
 */
class ReceivePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ReceivePanel(QWidget *parent = nullptr);

    void setPipeline(DataPipeline *pipeline);
    QString toPlainText() const;

public slots:
    void appendData(const QByteArray &data, bool isReceive, const QDateTime &ts);
    void appendSystemNote(const QString &note, bool success);
    void refreshDisplayMode();
    void clear();
    void search(const QString &text, bool regex, bool caseSensitive);
    void searchNext();
    void searchPrev();
    void clearSearchHighlight();
    void toggleBookmarkCurrent();
    void jumpNextBookmark();
    void jumpPrevBookmark();

    QJsonArray exportBookmarks() const;
    void importBookmarks(const QJsonArray &arr);

private:
    void setupUi();

    QListWidget *m_list = nullptr; // 临时用 QListWidget，正式应替换为自定义虚拟列表
    DataPipeline *m_pipeline = nullptr;

    // 搜索状态
    QString m_searchText;
    int m_searchIndex = -1;
    QList<int> m_searchMatches;
    QSet<int> m_bookmarks;
    int m_bookmarkCursor = -1;
};
