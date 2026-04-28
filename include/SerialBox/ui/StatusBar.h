/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/StatusBar.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
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
