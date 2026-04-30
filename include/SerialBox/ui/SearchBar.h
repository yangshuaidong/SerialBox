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
