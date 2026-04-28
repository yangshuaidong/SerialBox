/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/SearchBar.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/SearchBar.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QKeyEvent>

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SearchBar::setupUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("searchFloatingBar");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(44);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setObjectName("searchInput");
    m_input->setPlaceholderText("搜索...");
    m_input->setMinimumHeight(30);

    auto *prevBtn = new QPushButton("上", this);
    auto *nextBtn = new QPushButton("下", this);
    prevBtn->setObjectName("searchPrevBtn");
    nextBtn->setObjectName("searchNextBtn");
    prevBtn->setProperty("role", "info");
    nextBtn->setProperty("role", "info");
    prevBtn->setMinimumSize(44, 30);
    nextBtn->setMinimumSize(44, 30);

    m_regexCheck = new QCheckBox("正则", this);
    m_caseCheck = new QCheckBox("大小写", this);
    m_regexCheck->setObjectName("searchRegexCheck");
    m_caseCheck->setObjectName("searchCaseCheck");

    auto *closeBtn = new QPushButton("关闭", this);
    closeBtn->setObjectName("searchCloseBtn");
    closeBtn->setProperty("role", "danger");
    closeBtn->setMinimumSize(56, 30);

    layout->addWidget(m_input, 1);
    layout->addWidget(prevBtn);
    layout->addWidget(nextBtn);
    layout->addWidget(m_regexCheck);
    layout->addWidget(m_caseCheck);
    layout->addWidget(closeBtn);

    connect(m_input, &QLineEdit::textChanged, this, [this]()
            { emit searchRequested(m_input->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked()); });
    connect(m_input, &QLineEdit::returnPressed, this, [this]()
            { emit searchNext(); });
    connect(prevBtn, &QPushButton::clicked, this, &SearchBar::searchPrev);
    connect(nextBtn, &QPushButton::clicked, this, &SearchBar::searchNext);
    connect(closeBtn, &QPushButton::clicked, this, [this]()
            {
        hide();
        emit closed(); });
}

void SearchBar::showAndFocus()
{
    show();
    m_input->setFocus();
    m_input->selectAll();
}
