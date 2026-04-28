/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/SerialDialog.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QDialog>

class ConfigManager;
class QComboBox;

/**
 * SerialDialog — 串口连接弹窗（启动首次弹出）
 */
class SerialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SerialDialog(QWidget *parent = nullptr);

    void setConfigManager(ConfigManager *config);
    void refreshPorts();
    void updatePortList(const QStringList &ports);

signals:
    void connectRequested();
    void advancedSettingsRequested();

private:
    void setupUi();

    ConfigManager *m_config = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_flowCombo = nullptr;
};
