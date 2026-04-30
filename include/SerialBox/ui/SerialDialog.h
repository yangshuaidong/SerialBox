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
