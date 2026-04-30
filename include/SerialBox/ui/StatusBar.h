#pragma once
#include <QStatusBar>
#include <QLabel>

class SerialPortManager;

/**
 * StatusBar — 状态栏
 */
class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setSerialManager(SerialPortManager *manager);
    void setStatusMessage(const QString &msg, bool connected);

private:
    void setupUi();

    QLabel *m_portLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_rxLabel = nullptr;
    QLabel *m_txLabel = nullptr;
    QLabel *m_rxSpeedLabel = nullptr;
    QLabel *m_txSpeedLabel = nullptr;
    QLabel *m_uptimeLabel = nullptr;
    QLabel *m_encodingLabel = nullptr;

    SerialPortManager *m_serialManager = nullptr;
    quint64 m_lastRxBytes = 0;
    quint64 m_lastTxBytes = 0;
};
