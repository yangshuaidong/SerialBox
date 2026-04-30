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
