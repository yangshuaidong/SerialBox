/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/SerialSettings.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QDialog>

class ConfigManager;
class QTabWidget;
class QListWidget;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;

/**
 * SerialSettings — 串口设置面板（菜单入口，详细设置）
 *
 * Tab: 连接 | 参数 | 高级 | 预设
 */
class SerialSettings : public QDialog
{
    Q_OBJECT

public:
    explicit SerialSettings(QWidget *parent = nullptr);

    void refreshFromConfig(ConfigManager *config);
    void updatePortList(const QStringList &ports);

signals:
    void settingsApplied();

private:
    void setupUi();
    void populateDefaults();

    QTabWidget *m_tabs = nullptr;
    QListWidget *m_portList = nullptr;

    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_flowCombo = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;

    QCheckBox *m_autoReconnectCheck = nullptr;
    QSpinBox *m_retrySpin = nullptr;
    QSpinBox *m_intervalSpin = nullptr;

    QLineEdit *m_presetNameEdit = nullptr;
    QComboBox *m_presetCombo = nullptr;

    ConfigManager *m_configRef = nullptr;
};
