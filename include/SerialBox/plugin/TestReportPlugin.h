/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/plugin/TestReportPlugin.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once

#include <QDialog>

class SerialPortManager;
class QCheckBox;
class QComboBox;
class QLineEdit;

class TestReportPlugin : public QDialog
{
    Q_OBJECT

public:
    explicit TestReportPlugin(QWidget *parent = nullptr);

    void setSerialManager(SerialPortManager *manager);

private:
    void setupUi();
    void exportReport();

    SerialPortManager *m_serialManager = nullptr;
    QCheckBox *m_includeTimestamp = nullptr;
    QCheckBox *m_includePort = nullptr;
    QCheckBox *m_includeTraffic = nullptr;
    QCheckBox *m_includeUptime = nullptr;
    QLineEdit *m_titleEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
};
