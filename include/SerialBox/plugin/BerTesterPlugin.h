#pragma once

#include <QDialog>
#include <QByteArray>
#include <QMetaObject>

class SerialPortManager;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

class BerTesterPlugin : public QDialog
{
    Q_OBJECT

public:
    explicit BerTesterPlugin(QWidget *parent = nullptr);

    void setSerialManager(SerialPortManager *manager);

signals:
    void berDataCaptured(const QByteArray &data);

public slots:
    void onDataReceived(const QByteArray &data);

private:
    void setupUi();
    QByteArray currentPattern(bool *ok) const;
    void resetStats();
    void resetTxStats();
    void stopTxLoop();
    void sendOneFrame();
    void updateStatsUi();
    QString statsSummary() const;

    SerialPortManager *m_serialManager = nullptr;
    QLineEdit *m_patternEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_repeatSpin = nullptr;
    QSpinBox *m_incrementStartSpin = nullptr;
    QSpinBox *m_incrementLenSpin = nullptr;
    QLabel *m_txFramesLabel = nullptr;
    QLabel *m_txBytesLabel = nullptr;
    QLabel *m_txFailLabel = nullptr;
    QLabel *m_txRemainLabel = nullptr;
    QLabel *m_totalBitsLabel = nullptr;
    QLabel *m_errorBitsLabel = nullptr;
    QLabel *m_berLabel = nullptr;
    QLabel *m_rxBytesLabel = nullptr;
    QPushButton *m_startStopBtn = nullptr;
    QPushButton *m_sendToggleBtn = nullptr;
    QTimer *m_txTimer = nullptr;
    QMetaObject::Connection m_rxConn;
    QMetaObject::Connection m_disconnectedConn;

    bool m_running = false;
    bool m_txRunning = false;
    QByteArray m_expected;
    QByteArray m_txFrame;
    quint64 m_txFrames = 0;
    quint64 m_txBytes = 0;
    quint64 m_txFailures = 0;
    quint64 m_txRemainCycles = 0;
    quint64 m_totalBits = 0;
    quint64 m_errorBits = 0;
    quint64 m_rxBytes = 0;
};
