/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/core/BackPressureQueue.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QThread>

/**
 * BackPressureQueue — 大数据流控与背压队列
 *
 * 防高速数据流阻塞 UI 线程：
 * - 生产者（串口读线程）写入队列
 * - 消费者（定时器批量取）控制 UI 刷新频率
 * - 超出阈值时丢弃旧数据或暂停读取
 */
class BackPressureQueue : public QObject
{
    Q_OBJECT

public:
    explicit BackPressureQueue(QObject *parent = nullptr);

    void setMaxSize(int maxBytes);
    void setFlushIntervalMs(int ms);
    void start();
    void stop();

    void enqueue(const QByteArray &data);
    QByteArray dequeue(int maxBytes = -1);
    int size() const;
    int dropped() const { return m_dropped; }
    bool isFull() const;
    double usage() const;  // 0.0 ~ 1.0

signals:
    void readyToRead();

private:
    QQueue<QByteArray> m_queue;
    mutable QMutex m_mutex;
    int m_maxBytes = 10 * 1024 * 1024;  // 10MB
    int m_currentBytes = 0;
    int m_dropped = 0;
};
