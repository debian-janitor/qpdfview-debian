/*

Copyright 2014 Adam Reichold

This file is part of qpdfview.

qpdfview is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

qpdfview is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with qpdfview.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "thumbnailitem.h"

#include <QGraphicsSceneMouseEvent>
#include <qmath.h>
#include <QPainter>
#include <QWidget>

namespace qpdfview
{

ThumbnailItem::ThumbnailItem(Model::Page* page, int index, QGraphicsItem* parent) : PageItem(page, index, false, parent),
    m_text(QString::number(index + 1)),
    m_current(false)
{
    setAcceptHoverEvents(false);
}

QRectF ThumbnailItem::boundingRect() const
{
#if QT_VERSION >= QT_VERSION_CHECK(4,7,0)

    return PageItem::boundingRect().adjusted(0.0, 0.0, 0.0, 2.0 * m_text.size().height());

#else

    return PageItem::boundingRect().adjusted(0.0, 0.0, 0.0, 2.0 * QFontMetrics(QFont()).height());

#endif // QT_VERSION
}

void ThumbnailItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    PageItem::paint(painter, option, widget);

    const QRectF boundingRect = PageItem::boundingRect();

#if QT_VERSION >= QT_VERSION_CHECK(4,7,0)

    const QSizeF textSize = m_text.size();

    QPointF pos = boundingRect.bottomLeft();
    pos.rx() += 0.5 * (boundingRect.width() - textSize.width());
    pos.ry() += 0.5 * textSize.height();

    painter->drawStaticText(pos, m_text);

#else

    const QFontMetrics fontMetrics = QFontMetrics(QFont());

    QPointF pos = boundingRect.bottomLeft();
    pos.rx() += 0.5 * (boundingRect.width() - fontMetrics.width(m_text));
    pos.ry() += fontMetrics.height();

    painter->drawText(pos, m_text);

#endif // QT_VERSION

    if(m_current)
    {
        painter->save();

        painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        painter->fillRect(boundingRect, widget->palette().highlight());

        painter->restore();
    }
}

void ThumbnailItem::setCurrent(bool current)
{
    if(m_current != current)
    {
        m_current = current;

        update();
    }
}

void ThumbnailItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->modifiers() == Qt::NoModifier
            && (event->button() == Qt::LeftButton || event->button() == Qt::MidButton))
    {
        emit linkClicked(event->button() == Qt::MidButton, index() + 1);

        event->accept();
        return;
    }

    event->ignore();
}

void ThumbnailItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent*)
{
}

void ThumbnailItem::mouseMoveEvent(QGraphicsSceneMouseEvent*)
{
}

void ThumbnailItem::mouseReleaseEvent(QGraphicsSceneMouseEvent*)
{
}

void ThumbnailItem::contextMenuEvent(QGraphicsSceneContextMenuEvent*)
{
}

void ThumbnailItem::loadInteractiveElements()
{
    const qreal width = size().width() / 72.0 * 25.4;
    const qreal height = size().height() / 72.0 * 25.4;

    const qreal longEdge = qMax(width, height);
    const qreal shortEdge = qMin(width, height);

    QString paperSize;

    if(qAbs(longEdge - 279.4) <= 1.0 && qAbs(shortEdge - 215.9) <= 1.0)
    {
        paperSize = QLatin1String(" (Letter)");
    }
    else
    {
        qreal longEdgeA = 1189.0;
        qreal shortEdgeA = 841.0;

        qreal longEdgeB = 1414.0;
        qreal shortEdgeB = 1000.0;

        for(int i = 0; i <= 10; ++i)
        {
            if(qAbs(longEdge - longEdgeA) <= 1.0 && qAbs(shortEdge - shortEdgeA) <= 1.0)
            {
                paperSize = QString(" (A%1)").arg(i);
                break;
            }
            else if(qAbs(longEdge - longEdgeB) <= 1.0 && qAbs(shortEdge - shortEdgeB) <= 1.0)
            {
                paperSize = QString(" (B%1)").arg(i);
                break;
            }

            longEdgeA = shortEdgeA;
            shortEdgeA /= qSqrt(2.0);

            longEdgeB = shortEdgeB;
            shortEdgeB /= qSqrt(2.0);
        }
    }

    setToolTip(QString("%1 mm x %2 mm%3").arg(width, 0, 'f', 1).arg(height, 0, 'f', 1).arg(paperSize));
}

} // qpdfview
