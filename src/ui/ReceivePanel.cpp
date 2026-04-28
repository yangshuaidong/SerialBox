/*
 * BEGINNER_NOTE: 这是 SerialBox 的源码文件。
 * 文件路径: src/ui/ReceivePanel.cpp
 * 阅读建议: 先看文件顶部的类/函数声明，再顺着调用关系阅读。
 * 目标: 让零基础同学也能快速理解该文件在项目中的作用。
 */
#include "SerialBox/ui/ReceivePanel.h"
#include "SerialBox/core/DataPipeline.h"
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QDateTime>
#include <QRegularExpression>
#include <QBrush>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QJsonArray>
#include <algorithm>

namespace
{
    class ReceiveItemDelegate final : public QStyledItemDelegate
    {
    public:
        explicit ReceiveItemDelegate(QObject *parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            painter->save();

            const QRect fullRect = option.rect;
            const QRect textRect = fullRect.adjusted(8, 0, -8, 0);

            const QVariant bgVar = index.data(Qt::BackgroundRole);
            if (bgVar.canConvert<QBrush>())
            {
                const QBrush bg = qvariant_cast<QBrush>(bgVar);
                if (bg.style() != Qt::NoBrush)
                {
                    painter->fillRect(fullRect, bg);
                }
            }

            QString line = index.data(Qt::UserRole + 3).toString();
            QString ts = index.data(Qt::UserRole + 2).toDateTime().toString("HH:mm:ss.zzz");
            if (ts.isEmpty())
            {
                const int firstSpace = line.indexOf(' ');
                if (firstSpace > 0)
                {
                    ts = line.left(firstSpace);
                }
            }

            const bool hasDirection = index.data(Qt::UserRole + 1).isValid();
            const bool isReceive = index.data(Qt::UserRole + 1).toBool();

            QString marker;
            QString payload = line;
            if (!ts.isEmpty() && line.startsWith(ts + " "))
            {
                const int markerStart = ts.size() + 1;
                const int markerEnd = line.indexOf(' ', markerStart);
                if (markerEnd > markerStart)
                {
                    marker = line.mid(markerStart, markerEnd - markerStart);
                    payload = line.mid(markerEnd + 1);
                }
                else if (markerStart < line.size())
                {
                    marker = line.mid(markerStart);
                    payload.clear();
                }
            }

            if (hasDirection)
            {
                marker = isReceive ? QString::fromUtf8("收◄") : QString::fromUtf8("发►");
                if (payload.isEmpty() && !ts.isEmpty() && line.startsWith(ts + " "))
                {
                    const int secondSpace = line.indexOf(' ', ts.size() + 1);
                    if (secondSpace > 0 && secondSpace + 1 < line.size())
                    {
                        payload = line.mid(secondSpace + 1);
                    }
                }
            }
            else if (marker.isEmpty())
            {
                marker = "!";
            }

            const bool darkBackground = option.palette.base().color().lightness() < 96;
            QColor lineColor;
            if (hasDirection)
            {
                lineColor = isReceive ? QColor(darkBackground ? "#63B2FF" : "#0F52BA")
                                      : QColor(darkBackground ? "#FF8C96" : "#8B0000");
            }
            else
            {
                const QVariant fgVar = index.data(Qt::ForegroundRole);
                const QColor fallback("#4B607A");
                lineColor = fgVar.canConvert<QBrush>() ? qvariant_cast<QBrush>(fgVar).color() : fallback;
            }
            const QColor tsColor = lineColor;
            const QColor markerColor = lineColor;
            const QColor textColor = lineColor;

            QFont font = option.font;
            painter->setFont(font);
            const QFontMetrics fm(font);

            int x = textRect.left();
            const int tsWidth = fm.horizontalAdvance(QStringLiteral("00:00:00.000")) + 8;
            const int markerWidth = fm.horizontalAdvance(marker) + 12;

            painter->setPen(tsColor);
            painter->drawText(QRect(x, fullRect.top(), tsWidth, fullRect.height()), Qt::AlignLeft | Qt::AlignVCenter, ts);
            x += tsWidth;

            QFont arrowFont = font;
            arrowFont.setBold(true);
            painter->setFont(arrowFont);
            painter->setPen(markerColor);
            painter->drawText(QRect(x, fullRect.top(), markerWidth, fullRect.height()), Qt::AlignLeft | Qt::AlignVCenter, marker);
            x += markerWidth;

            painter->setFont(font);
            painter->setPen(textColor);
            painter->drawText(QRect(x, fullRect.top(), qMax(0, textRect.right() - x), fullRect.height()), Qt::AlignLeft | Qt::AlignVCenter, payload.trimmed());

            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            QSize sz = QStyledItemDelegate::sizeHint(option, index);
            sz.setHeight(18);
            return sz;
        }
    };
}

ReceivePanel::ReceivePanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ReceivePanel::setPipeline(DataPipeline *pipeline)
{
    m_pipeline = pipeline;
}

void ReceivePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setObjectName("receiveList");
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSpacing(0);
    m_list->setUniformItemSizes(false); // 不同行长
    m_list->setWordWrap(false);
    m_list->setTextElideMode(Qt::ElideNone);
    m_list->setItemDelegate(new ReceiveItemDelegate(m_list));

    layout->addWidget(m_list);
}

void ReceivePanel::appendData(const QByteArray &data, bool isReceive, const QDateTime &ts)
{
    QString tsStr = ts.toString("HH:mm:ss.zzz");
    QString dir = isReceive ? QString::fromUtf8("收◄") : QString::fromUtf8("发►");

    // 根据管线模式格式化
    QString text;
    if (m_pipeline && m_pipeline->displayMode() == DataPipeline::DisplayMode::Hex)
    {
        text = data.toHex(' ').toUpper();
    }
    else
    {
        text = QString::fromUtf8(data);
    }

    QString line = QString("%1 %2 %3").arg(tsStr, dir, text);

    auto *item = new QListWidgetItem(line);
    item->setData(Qt::UserRole, data); // 原始数据
    item->setData(Qt::UserRole + 1, isReceive);
    item->setData(Qt::UserRole + 2, ts);
    item->setData(Qt::UserRole + 3, line);

    int insertIndex = m_list->count();
    if (m_list->count() > 0)
    {
        const QDateTime lastTs = m_list->item(m_list->count() - 1)->data(Qt::UserRole + 2).toDateTime();
        // 大多数情况下新数据时间递增，直接追加以保持性能。
        if (lastTs.isValid() && ts < lastTs)
        {
            insertIndex = 0;
            for (int i = 0; i < m_list->count(); ++i)
            {
                const QDateTime curTs = m_list->item(i)->data(Qt::UserRole + 2).toDateTime();
                if (curTs.isValid() && ts < curTs)
                {
                    insertIndex = i;
                    break;
                }
            }
        }
    }

    if (insertIndex >= m_list->count())
    {
        m_list->addItem(item);
        m_list->scrollToBottom();
        return;
    }

    m_list->insertItem(insertIndex, item);

    if (!m_bookmarks.isEmpty())
    {
        QSet<int> shifted;
        for (int idx : std::as_const(m_bookmarks))
        {
            shifted.insert(idx >= insertIndex ? idx + 1 : idx);
        }
        m_bookmarks = std::move(shifted);
    }
}

void ReceivePanel::appendSystemNote(const QString &note, bool success)
{
    const QString tsStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString line = QString("%1 ! %2").arg(tsStr, note);
    auto *item = new QListWidgetItem(line);
    item->setData(Qt::UserRole + 2, QDateTime::currentDateTime());
    item->setData(Qt::UserRole + 3, line);
    item->setForeground(QBrush(success ? QColor("#16a34a") : QColor("#dc2626")));
    m_list->addItem(item);
    m_list->scrollToBottom();
}

void ReceivePanel::refreshDisplayMode()
{
    if (!m_list || !m_pipeline)
    {
        return;
    }

    for (int i = 0; i < m_list->count(); ++i)
    {
        auto *item = m_list->item(i);
        if (!item)
        {
            continue;
        }

        if (!item->data(Qt::UserRole).isValid() || !item->data(Qt::UserRole + 2).isValid())
        {
            continue;
        }

        const QByteArray raw = item->data(Qt::UserRole).toByteArray();
        const bool isReceive = item->data(Qt::UserRole + 1).toBool();
        const QDateTime ts = item->data(Qt::UserRole + 2).toDateTime();

        QString payload;
        if (m_pipeline->displayMode() == DataPipeline::DisplayMode::Hex)
        {
            payload = raw.toHex(' ').toUpper();
        }
        else
        {
            payload = QString::fromUtf8(raw);
        }

        const QString line = QString("%1 %2 %3")
                                 .arg(ts.toString("HH:mm:ss.zzz"), isReceive ? QString::fromUtf8("收◄") : QString::fromUtf8("发►"), payload);
        item->setText(line);
        item->setData(Qt::UserRole + 3, line);
    }
    m_list->viewport()->update();
}

void ReceivePanel::clear()
{
    m_list->clear();
    m_searchMatches.clear();
    m_searchIndex = -1;
    m_bookmarks.clear();
    m_bookmarkCursor = -1;
}

void ReceivePanel::search(const QString &text, bool regex, bool caseSensitive)
{
    clearSearchHighlight();
    m_searchText = text;
    m_searchMatches.clear();
    m_searchIndex = -1;

    if (text.isEmpty())
    {
        clearSearchHighlight();
        return;
    }

    for (int i = 0; i < m_list->count(); ++i)
    {
        auto *item = m_list->item(i);
        const QString lineText = item->data(Qt::UserRole + 3).toString();

        bool found = false;
        if (regex)
        {
            QRegularExpression re(text, caseSensitive ? QRegularExpression::NoPatternOption
                                                      : QRegularExpression::CaseInsensitiveOption);
            found = re.match(lineText).hasMatch();
        }
        else
        {
            found = lineText.contains(text, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        }

        if (found)
        {
            m_searchMatches.append(i);
        }
    }

    if (!m_searchMatches.isEmpty())
    {
        m_searchIndex = 0;
        for (int idx : m_searchMatches)
        {
            if (auto *matchItem = m_list->item(idx))
            {
                matchItem->setBackground(QBrush(QColor("#E8F1FB")));
            }
        }
        m_list->setCurrentRow(m_searchMatches.first());
        m_list->item(m_searchMatches.first())->setBackground(QBrush(QColor("#CFE2F8")));
        m_list->scrollToItem(m_list->item(m_searchMatches.first()));
    }
}

void ReceivePanel::searchNext()
{
    if (m_searchMatches.isEmpty())
        return;
    int oldRow = m_searchMatches[m_searchIndex];
    if (auto *oldItem = m_list->item(oldRow))
    {
        oldItem->setBackground(QBrush(QColor("#E8F1FB")));
    }
    m_searchIndex = (m_searchIndex + 1) % m_searchMatches.size();
    int row = m_searchMatches[m_searchIndex];
    m_list->setCurrentRow(row);
    if (auto *item = m_list->item(row))
    {
        item->setBackground(QBrush(QColor("#CFE2F8")));
    }
    m_list->scrollToItem(m_list->item(row));
}

void ReceivePanel::searchPrev()
{
    if (m_searchMatches.isEmpty())
        return;
    int oldRow = m_searchMatches[m_searchIndex];
    if (auto *oldItem = m_list->item(oldRow))
    {
        oldItem->setBackground(QBrush(QColor("#E8F1FB")));
    }
    m_searchIndex = (m_searchIndex - 1 + m_searchMatches.size()) % m_searchMatches.size();
    int row = m_searchMatches[m_searchIndex];
    m_list->setCurrentRow(row);
    if (auto *item = m_list->item(row))
    {
        item->setBackground(QBrush(QColor("#CFE2F8")));
    }
    m_list->scrollToItem(m_list->item(row));
}

void ReceivePanel::clearSearchHighlight()
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        if (auto *item = m_list->item(i))
        {
            item->setBackground(m_bookmarks.contains(i) ? QBrush(QColor("#fff3b0")) : QBrush(Qt::NoBrush));
        }
    }
    m_searchMatches.clear();
    m_searchIndex = -1;
}

QString ReceivePanel::toPlainText() const
{
    QStringList lines;
    lines.reserve(m_list->count());
    for (int i = 0; i < m_list->count(); ++i)
    {
        if (auto *item = m_list->item(i))
        {
            lines.append(item->data(Qt::UserRole + 3).toString());
        }
    }
    return lines.join('\n');
}

void ReceivePanel::toggleBookmarkCurrent()
{
    if (!m_list)
    {
        return;
    }
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_list->count())
    {
        return;
    }
    auto *item = m_list->item(row);
    if (!item)
    {
        return;
    }

    if (m_bookmarks.contains(row))
    {
        m_bookmarks.remove(row);
        item->setBackground(Qt::NoBrush);
    }
    else
    {
        m_bookmarks.insert(row);
        item->setBackground(QBrush(QColor("#fff3b0")));
    }
}

void ReceivePanel::jumpNextBookmark()
{
    if (m_bookmarks.isEmpty() || !m_list)
    {
        return;
    }
    QList<int> rows = m_bookmarks.values();
    std::sort(rows.begin(), rows.end());

    const int current = m_list->currentRow();
    int target = rows.first();
    for (int r : rows)
    {
        if (r > current)
        {
            target = r;
            break;
        }
    }
    m_list->setCurrentRow(target);
    m_list->scrollToItem(m_list->item(target));
    m_bookmarkCursor = target;
}

void ReceivePanel::jumpPrevBookmark()
{
    if (m_bookmarks.isEmpty() || !m_list)
    {
        return;
    }
    QList<int> rows = m_bookmarks.values();
    std::sort(rows.begin(), rows.end());

    const int current = m_list->currentRow();
    int target = rows.last();
    for (int i = rows.size() - 1; i >= 0; --i)
    {
        if (rows.at(i) < current)
        {
            target = rows.at(i);
            break;
        }
    }
    m_list->setCurrentRow(target);
    m_list->scrollToItem(m_list->item(target));
    m_bookmarkCursor = target;
}

QJsonArray ReceivePanel::exportBookmarks() const
{
    QJsonArray arr;
    QList<int> rows = m_bookmarks.values();
    std::sort(rows.begin(), rows.end());
    for (int r : rows)
    {
        arr.append(r);
    }
    return arr;
}

void ReceivePanel::importBookmarks(const QJsonArray &arr)
{
    if (!m_list)
    {
        return;
    }
    m_bookmarks.clear();
    for (const QJsonValue &v : arr)
    {
        const int row = v.toInt(-1);
        if (row >= 0 && row < m_list->count())
        {
            m_bookmarks.insert(row);
        }
    }
    clearSearchHighlight();
}
