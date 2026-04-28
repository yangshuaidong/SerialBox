/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/SearchBar.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QWidget>

class QLineEdit;
class QCheckBox;

/**
 * SearchBar — 搜索栏
 */
class SearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBar(QWidget *parent = nullptr);

    void showAndFocus();

signals:
    void searchRequested(const QString &text, bool regex, bool caseSensitive);
    void searchNext();
    void searchPrev();
    void closed();

private:
    void setupUi();

    QLineEdit *m_input = nullptr;
    QCheckBox *m_regexCheck = nullptr;
    QCheckBox *m_caseCheck = nullptr;
};
