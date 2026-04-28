#pragma once

#include <QDialog>
#include <QMetaObject>

class SerialPortManager;
class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QCheckBox;

class CrcCheckerPlugin : public QDialog
{
    Q_OBJECT

public:
    explicit CrcCheckerPlugin(QWidget *parent = nullptr);

    void setSerialManager(SerialPortManager *manager);

signals:
    void crcResultReady(const QString &message, bool pass);

public slots:
    void onDataReceived(const QByteArray &data);

private:
    void setupUi();
    quint16 calcCrc(const QByteArray &data) const;
    QByteArray parseHex(const QString &text, bool *ok) const;

    SerialPortManager *m_serialManager = nullptr;
    QMetaObject::Connection m_rxConn;

    QComboBox *m_algoCombo = nullptr;
    QComboBox *m_endianCombo = nullptr;
    QCheckBox *m_monitorCheck = nullptr;
    QListWidget *m_resultList = nullptr;

    QLineEdit *m_inputEdit = nullptr;
    QLabel *m_crcLabel = nullptr;
};
