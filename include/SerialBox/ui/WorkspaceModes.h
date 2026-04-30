#pragma once
#include <QWidget>

/**
 * WorkspaceModes — 多工作区模式切换
 *
 * 基础 / 专家 / 自动化
 */
class WorkspaceModes : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Basic, Expert, Automation };

    explicit WorkspaceModes(QWidget *parent = nullptr);

    Mode currentMode() const { return m_mode; }

signals:
    void modeChanged(Mode mode);

public slots:
    void setMode(Mode mode);

private:
    void setupUi();
    Mode m_mode = Mode::Basic;
};
