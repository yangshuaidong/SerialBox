/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/core/BackPressureQueue.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/core/BackPressureQueue.h"

BackPressureQueue::BackPressureQueue(QObject *parent)
    : QObject(parent)
{
}

void BackPressureQueue::setMaxSize(int maxBytes) { m_maxBytes = maxBytes; }

void BackPressureQueue::enqueue(const QByteArray &data)
{
    QMutexLocker lock(&m_mutex);

    // 背压：超限则丢弃最早的块
    while (m_currentBytes + data.size() > m_maxBytes && !m_queue.isEmpty()) {
        m_dropped += m_queue.head().size();
        m_currentBytes -= m_queue.dequeue().size();
    }

    m_queue.enqueue(data);
    m_currentBytes += data.size();
}

QByteArray BackPressureQueue::dequeue(int maxBytes)
{
    QMutexLocker lock(&m_mutex);
    if (m_queue.isEmpty()) return {};

    QByteArray result;
    int limit = (maxBytes < 0) ? m_currentBytes : maxBytes;

    while (!m_queue.isEmpty() && result.size() < limit) {
        auto chunk = m_queue.dequeue();
        m_currentBytes -= chunk.size();
        if (result.size() + chunk.size() <= limit) {
            result.append(chunk);
        } else {
            int take = limit - result.size();
            result.append(chunk.left(take));
            // 剩余放回队列头部
            m_queue.prepend(chunk.mid(take));
            m_currentBytes += chunk.size() - take;
        }
    }
    return result;
}

int BackPressureQueue::size() const
{
    QMutexLocker lock(&m_mutex);
    return m_currentBytes;
}

bool BackPressureQueue::isFull() const
{
    QMutexLocker lock(&m_mutex);
    return m_currentBytes >= m_maxBytes;
}

double BackPressureQueue::usage() const
{
    QMutexLocker lock(&m_mutex);
    return m_maxBytes > 0 ? static_cast<double>(m_currentBytes) / m_maxBytes : 0.0;
}
