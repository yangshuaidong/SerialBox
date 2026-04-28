/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: include/SerialBox/ui/WaveformWidget.h
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#pragma once
#include <QDialog>
#include <QVector>
#include <QPoint>
#include <array>

class QTimer;
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QSpinBox;

/**
 * WaveformWidget — 独立实时波形窗口
 *
 * 可与主窗口同步操作（非模态）
 * 支持多通道、缩放、平移、光标、导出、统计
 */
class WaveformWidget : public QDialog
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);

public slots:
    void feedData(const QByteArray &data);
    void clear();
    void togglePause();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class ProtocolMode
    {
        Auto = 0,
        RawRx = 1,
        Printf = 2
    };

    void setupUi();
    void updateWave();
    void parseBufferedFrames();
    bool parseFrameLine(const QByteArray &line, QVector<double> &channels) const;
    ProtocolMode protocolMode() const;
    bool looksLikePrintfChunk(const QByteArray &data) const;
    void appendFrameValues(const QVector<double> &values, const QString &statusText);
    void exportToCsv();
    void exportToPng();

    // 绘图区域计算
    QRect plotRect() const;
    void autoScaleY();

    QTimer *m_timer = nullptr;
    std::array<QVector<double>, 8> m_channels;
    std::array<QCheckBox *, 8> m_channelChecks = {nullptr};
    std::array<bool, 8> m_channelVisible = {true, true, false, false, false, false, false, false};
    QComboBox *m_protocolCombo = nullptr;
    QSpinBox *m_pointsSpin = nullptr;
    QByteArray m_lineBuffer;
    int m_channelCount = 2;
    int m_maxPoints = 600;
    bool m_paused = false;
    bool m_hasRealData = false;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_statsLabel = nullptr;

    // 缩放与平移
    double m_zoomFactor = 1.0;    // 1.0 = 全局，>1 = 放大
    int m_scrollOffset = 0;       // 水平偏移（从右侧开始）
    bool m_dragging = false;
    QPoint m_dragStart;
    int m_dragStartOffset = 0;

    // 光标
    bool m_cursorVisible = false;
    QPoint m_cursorPos;

    // 采样率估算
    qint64 m_lastFeedTime = 0;
    int m_feedCount = 0;
    double m_sampleRate = 0.0;
};
