/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/WaveformWidget.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/WaveformWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QRandomGenerator>
#include <QDateTime>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <QRegularExpression>
#include <QWheelEvent>
#include <QInputDialog>

namespace
{
    const QColor kChannelColors[8] = {
        QColor("#4A90D9"), QColor("#D98C4A"), QColor("#2D9A6A"), QColor("#8B6FD6"),
        QColor("#A0A33B"), QColor("#D65B96"), QColor("#4CA7A0"), QColor("#B8744A")};

    const int kMarginLeft = 60;
    const int kMarginRight = 100;
    const int kMarginTop = 10;
    const int kMarginBottom = 40;
}

WaveformWidget::WaveformWidget(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void WaveformWidget::setupUi()
{
    setWindowTitle("实时波形");
    setMinimumSize(700, 420);
    resize(800, 500);
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 工具栏 ──
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(40);
    toolbar->setObjectName("toolbar");
    auto *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 4, 12, 4);
    tbLayout->setSpacing(8);

    auto *title = new QLabel("📊 实时波形", toolbar);
    title->setStyleSheet("font-size: 13px; font-weight: 600;");
    tbLayout->addWidget(title);
    tbLayout->addSpacing(12);

    // 协议选择
    auto *protoLabel = new QLabel("协议", toolbar);
    tbLayout->addWidget(protoLabel);
    m_protocolCombo = new QComboBox(toolbar);
    m_protocolCombo->addItems({"自动", "直接接收(字节)", "printf(逗号通道)"});
    m_protocolCombo->setCurrentIndex(static_cast<int>(ProtocolMode::Auto));
    m_protocolCombo->setMinimumWidth(140);
    connect(m_protocolCombo, &QComboBox::currentIndexChanged, this, [this]()
            { m_lineBuffer.clear(); update(); });
    tbLayout->addWidget(m_protocolCombo);
    tbLayout->addSpacing(8);

    // 通道复选框
    for (int ch = 0; ch < 8; ++ch)
    {
        auto *check = new QCheckBox(QString("CH%1").arg(ch), toolbar);
        check->setChecked(ch < 2);
        check->setStyleSheet(
            QString("QCheckBox { color: %1; font-size: 11px; font-weight: 600; }"
                    "QCheckBox::indicator { width: 12px; height: 12px; }"
                    "QCheckBox::indicator:checked { background-color: %1; border: 1px solid %1; border-radius: 2px; }")
                .arg(kChannelColors[ch].name()));
        connect(check, &QCheckBox::toggled, this, [this, ch](bool on)
                { m_channelVisible[ch] = on; update(); });
        m_channelChecks[ch] = check;
        tbLayout->addWidget(check);
    }

    tbLayout->addStretch();

    // 暂停
    auto *pauseBtn = new QPushButton("⏸ 暂停", toolbar);
    pauseBtn->setCheckable(true);
    pauseBtn->setStyleSheet("QPushButton { padding: 4px 10px; }");
    connect(pauseBtn, &QPushButton::toggled, this, [pauseBtn, this](bool on)
            { m_paused = on; pauseBtn->setText(on ? "▶ 继续" : "⏸ 暂停"); });
    tbLayout->addWidget(pauseBtn);

    // 清空
    auto *clearBtn = new QPushButton("🗑 清空", toolbar);
    clearBtn->setStyleSheet("QPushButton { padding: 4px 10px; }");
    connect(clearBtn, &QPushButton::clicked, this, &WaveformWidget::clear);
    tbLayout->addWidget(clearBtn);

    // 导出
    auto *exportBtn = new QPushButton("💾 导出", toolbar);
    exportBtn->setStyleSheet("QPushButton { padding: 4px 10px; }");
    connect(exportBtn, &QPushButton::clicked, this, [this]()
            {
        QStringList items = {"CSV 数据文件 (*.csv)", "PNG 图片 (*.png)"};
        bool ok = false;
        QString choice = QInputDialog::getItem(this, "导出波形", "选择格式:", items, 0, false, &ok);
        if (!ok) return;
        if (choice.contains("csv")) exportToCsv();
        else exportToPng(); });
    tbLayout->addWidget(exportBtn);

    // 点数设置
    auto *ptsLabel = new QLabel("点数", toolbar);
    tbLayout->addWidget(ptsLabel);
    m_pointsSpin = new QSpinBox(toolbar);
    m_pointsSpin->setRange(100, 10000);
    m_pointsSpin->setValue(m_maxPoints);
    m_pointsSpin->setSingleStep(100);
    m_pointsSpin->setSuffix(" pts");
    connect(m_pointsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v)
            { m_maxPoints = v; update(); });
    tbLayout->addWidget(m_pointsSpin);

    mainLayout->addWidget(toolbar);

    // ── 波形绘图区 ──
    auto *canvasWidget = new QWidget(this);
    canvasWidget->setObjectName("canvas");
    canvasWidget->setMouseTracking(true);
    mainLayout->addWidget(canvasWidget, 1);

    // ── 状态栏 ──
    auto *statusbar = new QWidget(this);
    statusbar->setFixedHeight(24);
    auto *stLayout = new QHBoxLayout(statusbar);
    stLayout->setContentsMargins(12, 0, 12, 0);
    stLayout->setSpacing(16);

    m_statusLabel = new QLabel("就绪", statusbar);
    m_statusLabel->setStyleSheet("font-size: 10px; color: #666;");
    stLayout->addWidget(m_statusLabel);

    m_cursorLabel = new QLabel("", statusbar);
    m_cursorLabel->setStyleSheet("font-size: 10px; color: #4A90D9; font-weight: 600;");
    stLayout->addWidget(m_cursorLabel);

    m_statsLabel = new QLabel("", statusbar);
    m_statsLabel->setStyleSheet("font-size: 10px; color: #888;");
    stLayout->addWidget(m_statsLabel);

    mainLayout->addWidget(statusbar);

    // 定时刷新
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]()
            { if (!m_paused) updateWave(); });
    m_timer->start(50);

    // 预填模拟数据
    for (int i = 0; i < m_maxPoints; ++i)
    {
        m_channels[0].append(25.0 + QRandomGenerator::global()->bounded(3.0));
        m_channels[1].append(48.0 + QRandomGenerator::global()->bounded(5.0));
    }
}

void WaveformWidget::feedData(const QByteArray &data)
{
    if (m_paused || data.isEmpty())
    {
        return;
    }

    // 采样率估算
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastFeedTime > 0 && (now - m_lastFeedTime) < 2000) {
        m_feedCount++;
        qint64 elapsed = now - m_lastFeedTime;
        if (elapsed > 500) {
            m_sampleRate = m_feedCount * 1000.0 / elapsed;
            m_feedCount = 0;
            m_lastFeedTime = now;
        }
    } else {
        m_lastFeedTime = now;
        m_feedCount = 1;
    }

    const ProtocolMode mode = protocolMode();
    if (mode == ProtocolMode::RawRx)
    {
        m_hasRealData = true;
        m_channelCount = 1;
        for (unsigned char b : data)
        {
            m_channels[0].append(static_cast<double>(b));
            while (m_channels[0].size() > m_maxPoints)
                m_channels[0].removeFirst();
        }
        update();
        return;
    }

    if (mode == ProtocolMode::Printf || looksLikePrintfChunk(data) || !m_lineBuffer.isEmpty())
    {
        if (mode == ProtocolMode::Printf)
        {
            m_lineBuffer.append(data);
            parseBufferedFrames();
            return;
        }

        QByteArray probe = m_lineBuffer;
        probe.append(data);
        if (looksLikePrintfChunk(probe))
        {
            m_lineBuffer = probe;
            if (m_lineBuffer.size() > 8192)
                m_lineBuffer.clear();
            parseBufferedFrames();
            return;
        }
        m_lineBuffer.clear();
    }

    m_hasRealData = true;
    m_channelCount = 1;
    for (unsigned char b : data)
    {
        m_channels[0].append(static_cast<double>(b));
        while (m_channels[0].size() > m_maxPoints)
            m_channels[0].removeFirst();
    }
    update();
}

void WaveformWidget::clear()
{
    for (auto &ch : m_channels)
        ch.clear();
    m_hasRealData = false;
    m_lineBuffer.clear();
    m_zoomFactor = 1.0;
    m_scrollOffset = 0;
    m_sampleRate = 0.0;
    m_feedCount = 0;
    m_lastFeedTime = 0;
    m_cursorLabel->clear();
    m_statsLabel->clear();
    update();
}

void WaveformWidget::togglePause()
{
    m_paused = !m_paused;
}

// ═══════════════════════════════════════════
//  缩放 & 平移
// ═══════════════════════════════════════════

void WaveformWidget::wheelEvent(QWheelEvent *event)
{
    QRect pr = plotRect();
    if (!pr.contains(event->position().toPoint())) {
        QDialog::wheelEvent(event);
        return;
    }

    double delta = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    double newZoom = qBound(1.0, m_zoomFactor * delta, 20.0);
    m_zoomFactor = newZoom;
    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragStartOffset = m_scrollOffset;
        setCursor(Qt::ClosedHandCursor);
    }
    QDialog::mousePressEvent(event);
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
    QDialog::mouseReleaseEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    QRect pr = plotRect();
    m_cursorPos = event->pos();
    m_cursorVisible = pr.contains(m_cursorPos);

    if (m_dragging && m_zoomFactor > 1.0) {
        int dx = event->pos().x() - m_dragStart.x();
        // 拖动方向：向右拖 = 看更早的数据
        m_scrollOffset = m_dragStartOffset + dx;
        // 限制范围
        int maxDataLen = 0;
        for (int ch = 0; ch < m_channelCount; ++ch)
            maxDataLen = qMax(maxDataLen, m_channels[ch].size());
        int visiblePoints = static_cast<int>(maxDataLen / m_zoomFactor);
        m_scrollOffset = qBound(0, m_scrollOffset, qMax(0, maxDataLen - visiblePoints));
    }

    // 更新光标数值显示
    if (m_cursorVisible) {
        int plotW = pr.width() - kMarginLeft - kMarginRight;
        if (plotW > 0) {
            double relX = (m_cursorPos.x() - pr.x() - kMarginLeft) / static_cast<double>(plotW);
            int maxDataLen = 0;
            for (int ch = 0; ch < m_channelCount; ++ch)
                maxDataLen = qMax(maxDataLen, m_channels[ch].size());

            int visiblePoints = static_cast<int>(maxDataLen / m_zoomFactor);
            int startIdx = qMax(0, maxDataLen - visiblePoints - m_scrollOffset);
            int sampleIdx = startIdx + static_cast<int>(relX * visiblePoints);

            QStringList vals;
            for (int ch = 0; ch < m_channelCount; ++ch) {
                if (m_channelVisible[ch] && sampleIdx >= 0 && sampleIdx < m_channels[ch].size()) {
                    vals.append(QString("CH%1=%2").arg(ch).arg(m_channels[ch][sampleIdx], 0, 'f', 2));
                }
            }
            m_cursorLabel->setText(QString("▼ 样本#%1  %2").arg(sampleIdx).arg(vals.join("  ")));
        }
    } else {
        m_cursorLabel->clear();
    }

    update();
    QDialog::mouseMoveEvent(event);
}

void WaveformWidget::leaveEvent(QEvent *event)
{
    m_cursorVisible = false;
    m_cursorLabel->clear();
    update();
    QDialog::leaveEvent(event);
}

// ═══════════════════════════════════════════
//  绘图
// ═══════════════════════════════════════════

QRect WaveformWidget::plotRect() const
{
    auto *canvas = findChild<QWidget *>("canvas");
    return canvas ? canvas->geometry() : rect();
}

void WaveformWidget::updateWave()
{
    if (!m_hasRealData)
    {
        double t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        m_channels[0].append(25.0 + qSin(t * 0.5) * 3.0 + qSin(t * 1.2) * 1.5);
        m_channels[1].append(48.0 + qSin(t * 0.3) * 5.0 + qCos(t * 0.8) * 3.0);
        if (m_channels[0].size() > m_maxPoints)
        {
            m_channels[0].removeFirst();
            m_channels[1].removeFirst();
        }
    }
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect pr = plotRect();
    int W = pr.width();
    int H = pr.height();
    int ox = pr.x();
    int oy = pr.y();

    // 背景
    p.fillRect(pr, QColor("#FAFAFA"));

    int plotW = W - kMarginLeft - kMarginRight;
    int plotH = H - kMarginTop - kMarginBottom;

    if (plotW <= 10 || plotH <= 10)
        return;

    // ── 数据范围 ──
    int maxDataLen = 0;
    double yMin = 0.0, yMax = 1.0;
    bool hasData = false;
    for (int ch = 0; ch < m_channelCount; ++ch)
    {
        if (!m_channelVisible[ch]) continue;
        maxDataLen = qMax(maxDataLen, m_channels[ch].size());
        for (double v : m_channels[ch])
        {
            if (!hasData) { yMin = yMax = v; hasData = true; }
            else { yMin = qMin(yMin, v); yMax = qMax(yMax, v); }
        }
    }

    if (maxDataLen < 2)
    {
        p.setPen(QColor("#AAA"));
        p.setFont(QFont("sans-serif", 12));
        p.drawText(pr, Qt::AlignCenter, "等待数据...");
        return;
    }

    if (qFuzzyCompare(yMin, yMax)) { yMin -= 1.0; yMax += 1.0; }
    double yPad = (yMax - yMin) * 0.05;
    yMin -= yPad;
    yMax += yPad;
    double yRange = yMax - yMin;

    // ── 缩放：计算可见样本范围 ──
    int visiblePoints = qMax(2, static_cast<int>(maxDataLen / m_zoomFactor));
    int startIdx = qMax(0, maxDataLen - visiblePoints - m_scrollOffset);
    int endIdx = qMin(maxDataLen, startIdx + visiblePoints);

    // ── 网格 ──
    p.setPen(QPen(QColor("#E0E0E0"), 0.5, Qt::DotLine));
    for (int y = 0; y <= 5; ++y)
    {
        int py = oy + kMarginTop + plotH * y / 5;
        p.drawLine(ox + kMarginLeft, py, ox + W - kMarginRight, py);
    }
    for (int x = 0; x <= 8; ++x)
    {
        int px = ox + kMarginLeft + plotW * x / 8;
        p.drawLine(px, oy + kMarginTop, px, oy + H - kMarginBottom);
    }

    // ── 坐标轴 ──
    p.setPen(QPen(QColor("#9AA4B2"), 1.2));
    int xAxisY = oy + H - kMarginBottom;
    int yAxisX = ox + kMarginLeft;
    p.drawLine(yAxisX, oy + kMarginTop, yAxisX, xAxisY);
    p.drawLine(yAxisX, xAxisY, ox + W - kMarginRight, xAxisY);

    // 箭头
    p.drawLine(ox + W - kMarginRight, xAxisY, ox + W - kMarginRight - 5, xAxisY - 3);
    p.drawLine(ox + W - kMarginRight, xAxisY, ox + W - kMarginRight - 5, xAxisY + 3);
    p.drawLine(yAxisX, oy + kMarginTop, yAxisX - 3, oy + kMarginTop + 5);
    p.drawLine(yAxisX, oy + kMarginTop, yAxisX + 3, oy + kMarginTop + 5);

    // ── Y轴标签 ──
    p.setPen(QColor("#7A7A7A"));
    p.setFont(QFont("monospace", 8));
    for (int i = 0; i <= 5; ++i)
    {
        double val = yMax - (yMax - yMin) * i / 5.0;
        int py = oy + kMarginTop + plotH * i / 5;
        p.drawText(ox + 2, py - 4, kMarginLeft - 6, 16,
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 1));
    }

    // ── X轴标签 ──
    p.drawText(ox + W - kMarginRight + 4, xAxisY + 4, "Samples");
    for (int i = 0; i <= 4; ++i)
    {
        int sampleNum = startIdx + (endIdx - startIdx) * i / 4;
        int px = ox + kMarginLeft + plotW * i / 4;
        p.drawText(px - 20, xAxisY + 6, 40, 14,
                   Qt::AlignHCenter | Qt::AlignTop, QString::number(sampleNum));
    }

    // Y轴标题
    p.save();
    p.translate(ox + 10, oy + kMarginTop + plotH / 2);
    p.rotate(-90);
    p.drawText(-20, 0, "Value");
    p.restore();

    // ── 波形曲线 ──
    for (int ch = 0; ch < m_channelCount; ++ch)
    {
        if (!m_channelVisible[ch] || m_channels[ch].size() < 2)
            continue;

        p.setPen(QPen(kChannelColors[ch % 8], 1.8));
        QPainterPath path;

        int drawStart = qMax(0, qMin(startIdx, m_channels[ch].size()));
        int drawEnd = qMin(endIdx, m_channels[ch].size());

        for (int i = drawStart; i < drawEnd; ++i)
        {
            double x = ox + kMarginLeft + plotW * (i - startIdx) / static_cast<double>(qMax(1, visiblePoints - 1));
            double y = oy + kMarginTop + plotH * (1.0 - (m_channels[ch][i] - yMin) / yRange);
            i == drawStart ? path.moveTo(x, y) : path.lineTo(x, y);
        }
        p.drawPath(path);
    }

    // ── 光标十字线 ──
    if (m_cursorVisible)
    {
        int cx = m_cursorPos.x();
        int cy = m_cursorPos.y();
        if (cx >= ox + kMarginLeft && cx <= ox + W - kMarginRight &&
            cy >= oy + kMarginTop && cy <= xAxisY)
        {
            p.setPen(QPen(QColor("#E74C3C"), 1, Qt::DashLine));
            p.drawLine(cx, oy + kMarginTop, cx, xAxisY);
            p.drawLine(ox + kMarginLeft, cy, ox + W - kMarginRight, cy);

            // Y值标注
            double yVal = yMax - (cy - oy - kMarginTop) / static_cast<double>(plotH) * yRange;
            p.setPen(QColor("#E74C3C"));
            p.setFont(QFont("monospace", 9, QFont::Bold));
            p.drawText(ox + W - kMarginRight + 2, cy - 6, 60, 14,
                       Qt::AlignLeft | Qt::AlignVCenter, QString::number(yVal, 'f', 1));
        }
    }

    // ── 图例（右侧色块） ──
    int legendX = ox + W - kMarginRight + 12;
    int legendY = oy + kMarginTop + 8;
    int legendRow = 0;
    for (int ch = 0; ch < m_channelCount; ++ch)
    {
        if (!m_channelVisible[ch]) continue;
        // 色块
        p.fillRect(legendX, legendY + legendRow * 16, 12, 10, kChannelColors[ch % 8]);
        p.setPen(kChannelColors[ch % 8]);
        p.setFont(QFont("sans-serif", 9, QFont::Bold));
        p.drawText(legendX + 16, legendY + legendRow * 16 + 9, QString("CH%1").arg(ch));
        ++legendRow;
    }

    // ── 缩放比例指示 ──
    if (m_zoomFactor > 1.01) {
        p.setPen(QColor("#999"));
        p.setFont(QFont("sans-serif", 8));
        p.drawText(ox + W - kMarginRight - 60, oy + kMarginTop + 2,
                   QString("×%1").arg(m_zoomFactor, 0, 'f', 1));
    }

    // ── 统计信息更新 ──
    QStringList statsParts;
    for (int ch = 0; ch < m_channelCount; ++ch) {
        if (!m_channelVisible[ch] || m_channels[ch].isEmpty()) continue;
        double minV = m_channels[ch].first(), maxV = m_channels[ch].first(), sum = 0;
        for (double v : m_channels[ch]) {
            minV = qMin(minV, v);
            maxV = qMax(maxV, v);
            sum += v;
        }
        double avg = sum / m_channels[ch].size();
        statsParts.append(QString("CH%1: [min:%2 avg:%3 max:%4]")
                              .arg(ch).arg(minV, 0, 'f', 1)
                              .arg(avg, 0, 'f', 1).arg(maxV, 0, 'f', 1));
    }
    if (m_statsLabel) m_statsLabel->setText(statsParts.join("  "));

    // ── 状态栏信息 ──
    if (m_statusLabel) {
        QString rateText = m_sampleRate > 0 ? QString(" · %1 sps").arg(m_sampleRate, 0, 'f', 0) : "";
        m_statusLabel->setText(QString("%1通道 · 显示%2/%3点%4 · 滚轮缩放 · 拖拽平移")
                                   .arg(m_channelCount).arg(visiblePoints).arg(maxDataLen).arg(rateText));
    }
}

// ═══════════════════════════════════════════
//  导出
// ═══════════════════════════════════════════

void WaveformWidget::exportToCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "导出波形数据",
        "waveform.csv", "CSV 文件 (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件。");
        return;
    }

    QTextStream out(&f);
    // 表头
    QStringList headers = {"sample"};
    for (int ch = 0; ch < m_channelCount; ++ch)
        headers.append(QString("CH%1").arg(ch));
    out << headers.join(",") << "\n";

    // 数据
    int maxSize = 0;
    for (int ch = 0; ch < m_channelCount; ++ch)
        maxSize = qMax(maxSize, m_channels[ch].size());

    for (int i = 0; i < maxSize; ++i) {
        QStringList row = {QString::number(i)};
        for (int ch = 0; ch < m_channelCount; ++ch) {
            if (i < m_channels[ch].size())
                row.append(QString::number(m_channels[ch][i], 'f', 6));
            else
                row.append("");
        }
        out << row.join(",") << "\n";
    }

    QMessageBox::information(this, "导出成功",
        QString("已导出 %1 个样本点到:\n%2").arg(maxSize).arg(path));
}

void WaveformWidget::exportToPng()
{
    const QString path = QFileDialog::getSaveFileName(this, "导出波形图片",
        "waveform.png", "PNG 图片 (*.png)");
    if (path.isEmpty()) return;

    QPixmap pixmap(size());
    render(&pixmap);
    if (pixmap.save(path, "PNG")) {
        QMessageBox::information(this, "导出成功",
            QString("波形图片已保存到:\n%1").arg(path));
    } else {
        QMessageBox::warning(this, "导出失败", "无法保存图片。");
    }
}

// ═══════════════════════════════════════════
//  辅助
// ═══════════════════════════════════════════

void WaveformWidget::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void WaveformWidget::parseBufferedFrames()
{
    while (true)
    {
        int idxN = m_lineBuffer.indexOf('\n');
        int idxR = m_lineBuffer.indexOf('\r');
        int cut = -1;
        if (idxN >= 0 && idxR >= 0)
            cut = qMin(idxN, idxR);
        else
            cut = qMax(idxN, idxR);
        if (cut < 0)
            break;

        QByteArray line = m_lineBuffer.left(cut);
        int consume = cut + 1;
        while (consume < m_lineBuffer.size() &&
               (m_lineBuffer[consume] == '\n' || m_lineBuffer[consume] == '\r'))
            ++consume;
        m_lineBuffer.remove(0, consume);

        QVector<double> values;
        if (parseFrameLine(line, values))
            appendFrameValues(values, QString("printf 帧 · %1 通道").arg(values.size()));
    }
}

void WaveformWidget::appendFrameValues(const QVector<double> &values, const QString &statusText)
{
    if (values.isEmpty()) return;

    m_hasRealData = true;
    m_channelCount = qMin(8, values.size());
    for (int ch = 0; ch < m_channelCount; ++ch)
    {
        m_channels[ch].append(values[ch]);
        while (m_channels[ch].size() > m_maxPoints)
            m_channels[ch].removeFirst();
    }
    for (int ch = m_channelCount; ch < 8; ++ch)
    {
        if (!m_channels[ch].isEmpty())
        {
            m_channels[ch].append(m_channels[ch].last());
            while (m_channels[ch].size() > m_maxPoints)
                m_channels[ch].removeFirst();
        }
    }
    update();
}

WaveformWidget::ProtocolMode WaveformWidget::protocolMode() const
{
    if (!m_protocolCombo) return ProtocolMode::Auto;
    return static_cast<ProtocolMode>(m_protocolCombo->currentIndex());
}

bool WaveformWidget::looksLikePrintfChunk(const QByteArray &data) const
{
    if (data.isEmpty()) return false;

    int printableCount = 0;
    for (unsigned char c : data)
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c <= 126))
            ++printableCount;

    double ratio = static_cast<double>(printableCount) / data.size();
    if (ratio < 0.85) return false;

    QString text = QString::fromLatin1(data);
    if (text.contains('\n') || text.contains('\r')) return true;

    static const QRegularExpression kNumeric(
        "(?:^|\\s)(?:[A-Za-z_][A-Za-z0-9_]*\\s*:\\s*)?[-+]?\\d+(?:\\.\\d+)?(?:\\s*,\\s*[-+]?\\d+(?:\\.\\d+)?)+(?:$|\\s)");
    return kNumeric.match(text).hasMatch();
}

bool WaveformWidget::parseFrameLine(const QByteArray &line, QVector<double> &channels) const
{
    channels.clear();
    QString text = QString::fromUtf8(line).trimmed();
    if (text.isEmpty()) return false;

    int colon = text.indexOf(':');
    QString payload = text;
    if (colon >= 0)
    {
        QString prefix = text.left(colon).trimmed();
        payload = text.mid(colon + 1).trimmed();
        if (prefix.compare("image", Qt::CaseInsensitive) == 0)
            return false;
    }
    if (payload.isEmpty()) return false;

    QStringList parts = payload.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    for (int i = 0; i < parts.size() && i < 8; ++i)
    {
        bool ok = false;
        double v = parts[i].trimmed().toDouble(&ok);
        if (!ok) return false;
        channels.append(v);
    }
    return !channels.isEmpty();
}
