/*

Copyright 2012 Adam Reichold

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

#include "pageitem.h"

QCache< PageItem*, QImage > PageItem::s_cache(32 * 1024 * 1024);

bool PageItem::s_decoratePages = true;
bool PageItem::s_decorateLinks = true;
bool PageItem::s_decorateFormFields = true;

bool PageItem::s_invertColors = false;

Qt::KeyboardModifiers PageItem::s_copyModifiers = Qt::ShiftModifier;
Qt::KeyboardModifiers PageItem::s_annotateModifiers = Qt::ControlModifier;

int PageItem::cacheSize()
{
    return s_cache.totalCost();
}

void PageItem::setCacheSize(int cacheSize)
{
    s_cache.setMaxCost(cacheSize);
}

bool PageItem::decoratePages()
{
    return s_decoratePages;
}

void PageItem::setDecoratePages(bool decoratePages)
{
    s_decoratePages = decoratePages;
}

bool PageItem::decorateLinks()
{
    return s_decorateLinks;
}

void PageItem::setDecorateLinks(bool decorateLinks)
{
    s_decorateLinks = decorateLinks;
}

bool PageItem::decorateFormFields()
{
    return s_decorateFormFields;
}

void PageItem::setDecorateFormFields(bool decorateFormFields)
{
    s_decorateFormFields = decorateFormFields;
}

bool PageItem::invertColors()
{
    return s_invertColors;
}

void PageItem::setInvertColors(bool invertColors)
{
    s_invertColors = invertColors;
}

const Qt::KeyboardModifiers& PageItem::copyModifiers()
{
    return s_copyModifiers;
}

void PageItem::setCopyModifiers(const Qt::KeyboardModifiers& copyModifiers)
{
    s_copyModifiers = copyModifiers;
}

const Qt::KeyboardModifiers& PageItem::annotateModifiers()
{
    return s_annotateModifiers;
}

void PageItem::setAnnotateModifiers(const Qt::KeyboardModifiers& annotateModifiers)
{
    s_annotateModifiers = annotateModifiers;
}

PageItem::PageItem(QMutex* mutex, Poppler::Page* page, int index, QGraphicsItem* parent) : QGraphicsObject(parent),
    m_mutex(0),
    m_page(0),
    m_index(-1),
    m_size(),
    m_links(),
    m_annotations(),
    m_highlights(),
    m_rubberBandMode(ModifiersMode),
    m_rubberBand(),
    m_physicalDpiX(72),
    m_physicalDpiY(72),
    m_scaleFactor(1.0),
    m_rotation(Poppler::Page::Rotate0),
    m_transform(),
    m_normalizedTransform(),
    m_boundingRect(),
    m_image(),
    m_render(0)
{
    setAcceptHoverEvents(true);

    m_render = new QFutureWatcher< void >(this);
    connect(m_render, SIGNAL(finished()), SLOT(on_render_finished()));

    connect(this, SIGNAL(imageReady(int,int,qreal,Poppler::Page::Rotation,bool,QImage)), SLOT(on_imageReady(int,int,qreal,Poppler::Page::Rotation,bool,QImage)));

    m_mutex = mutex;
    m_page = page;

    m_index = index;
    m_size = m_page->pageSizeF();

    foreach(Poppler::Link* link, m_page->links())
    {
        if(link->linkType() == Poppler::Link::Goto)
        {
            if(!static_cast< Poppler::LinkGoto* >(link)->isExternal())
            {
                m_links.append(link);
                continue;
            }
        }
        else if(link->linkType() == Poppler::Link::Browse)
        {
            m_links.append(link);
            continue;
        }

        delete link;
    }

    foreach(Poppler::Annotation* annotation, m_page->annotations())
    {
        if(annotation->subType() == Poppler::Annotation::AText || annotation->subType() == Poppler::Annotation::AHighlight)
        {
            m_annotations.append(annotation);
            continue;
        }

        delete annotation;
    }

    foreach(Poppler::FormField* formField, m_page->formFields())
    {
        switch(formField->type())
        {
        case Poppler::FormField::FormSignature:
            delete formField;
            break;
        case Poppler::FormField::FormText:
            switch(static_cast< Poppler::FormFieldText* >(formField)->textType())
            {
            case Poppler::FormFieldText::FileSelect:
                delete formField;
                break;
            case Poppler::FormFieldText::Normal:
            case Poppler::FormFieldText::Multiline:
                m_formFields.append(formField);
                break;
            }

            break;
        case Poppler::FormField::FormChoice:
            m_formFields.append(formField);
            break;
        case Poppler::FormField::FormButton:
            switch(static_cast< Poppler::FormFieldButton* >(formField)->buttonType())
            {
            case Poppler::FormFieldButton::Push:
                delete formField;
                break;
            case Poppler::FormFieldButton::CheckBox:
            case Poppler::FormFieldButton::Radio:
                m_formFields.append(formField);
                break;
            }

            break;
        }
    }

    prepareGeometry();
}

PageItem::~PageItem()
{
    m_render->cancel();
    m_render->waitForFinished();

    s_cache.remove(this);

    qDeleteAll(m_links);
    qDeleteAll(m_annotations);

    if(m_page != 0)
    {
        delete m_page;
    }
}

QRectF PageItem::boundingRect() const
{
    return m_boundingRect;
}

void PageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QImage image;

    if(s_cache.contains(this))
    {
        image = *s_cache.object(this);
    }
    else
    {
        if(!m_image.isNull())
        {
            image = m_image;
            m_image = QImage();

            s_cache.insert(this, new QImage(image), image.byteCount());
        }
        else
        {
            startRender();
        }
    }

    // page

    if(s_decoratePages)
    {
        painter->fillRect(m_boundingRect, QBrush(s_invertColors ? Qt::black : Qt::white));

        painter->drawImage(m_boundingRect.topLeft(), image);

        painter->setPen(QPen(s_invertColors ? Qt::white : Qt::black));
        painter->drawRect(m_boundingRect);
    }
    else
    {
        painter->drawImage(m_boundingRect.topLeft(), image);
    }

    // links

    if(s_decorateLinks)
    {
        painter->save();

        painter->setTransform(m_normalizedTransform, true);
        painter->setPen(QPen(Qt::red));

        foreach(Poppler::Link* link, m_links)
        {
            painter->drawRect(link->linkArea().normalized());
        }

        painter->restore();
    }

    // form fields

    if(s_decorateFormFields)
    {
        painter->save();

        painter->setTransform(m_normalizedTransform, true);
        painter->setPen(QPen(Qt::blue));

        foreach(Poppler::FormField* formField, m_formFields)
        {
            if(formField->isVisible())
            {
                painter->drawRect(formField->rect().normalized());
            }
        }

        painter->restore();
    }

    // highlights

    painter->save();

    painter->setTransform(m_transform, true);

    QColor highlightColor = QApplication::palette().color(QPalette::Highlight);

    highlightColor.setAlpha(127);

    foreach(QRectF highlight, m_highlights)
    {
        painter->fillRect(highlight.normalized(), QBrush(highlightColor));
    }

    painter->restore();

    // rubber band

    if(!m_rubberBand.isNull())
    {
        QPen pen;
        pen.setColor(s_invertColors ? Qt::white : Qt::black);
        pen.setStyle(Qt::DashLine);

        painter->setPen(pen);
        painter->drawRect(m_rubberBand);
    }
}

int PageItem::index() const
{
    return m_index;
}

QSizeF PageItem::size() const
{
    return m_size;
}

const QList< QRectF >& PageItem::highlights() const
{
    return m_highlights;
}

void PageItem::setHighlights(const QList< QRectF >& highlights, int duration)
{
    m_highlights = highlights;

    update();

    if(duration > 0)
    {
        QTimer::singleShot(duration, this, SLOT(clearHighlights()));
    }
}

PageItem::RubberBandMode PageItem::rubberBandMode() const
{
    return m_rubberBandMode;
}

void PageItem::setRubberBandMode(PageItem::RubberBandMode rubberBandMode)
{
    m_rubberBandMode = rubberBandMode;

    if(m_rubberBandMode == ModifiersMode)
    {
        unsetCursor();
    }
    else
    {
        setCursor(Qt::CrossCursor);
    }
}

int PageItem::physicalDpiX() const
{
    return m_physicalDpiX;
}

int PageItem::physicalDpiY() const
{
    return m_physicalDpiY;
}

void PageItem::setPhysicalDpi(int physicalDpiX, int physicalDpiY)
{
    if(m_physicalDpiX != physicalDpiX || m_physicalDpiY != physicalDpiY)
    {
        refresh();

        m_physicalDpiX = physicalDpiX;
        m_physicalDpiY = physicalDpiY;

        prepareGeometryChange();
        prepareGeometry();
    }
}

qreal PageItem::scaleFactor() const
{
    return m_scaleFactor;
}

void PageItem::setScaleFactor(qreal scaleFactor)
{
    if(m_scaleFactor != scaleFactor)
    {
        refresh();

        m_scaleFactor = scaleFactor;

        prepareGeometryChange();
        prepareGeometry();
    }
}

Poppler::Page::Rotation PageItem::rotation() const
{
    return m_rotation;
}

void PageItem::setRotation(Poppler::Page::Rotation rotation)
{
    if(m_rotation != rotation)
    {
        refresh();

        m_rotation = rotation;

        prepareGeometryChange();
        prepareGeometry();
    }
}

const QTransform& PageItem::transform() const
{
    return m_transform;
}

const QTransform& PageItem::normalizedTransform() const
{
    return m_normalizedTransform;
}

void PageItem::refresh()
{
    cancelRender();

    s_cache.remove(this);

    update();
}

void PageItem::clearHighlights()
{
    m_highlights.clear();

    update();
}

void PageItem::startRender(bool prefetch)
{
    if(!m_render->isRunning())
    {
        m_render->setFuture(QtConcurrent::run(this, &PageItem::render, m_physicalDpiX, m_physicalDpiY, m_scaleFactor, m_rotation, prefetch));
    }
}

void PageItem::cancelRender()
{
    m_render->cancel();

    m_image = QImage();
}

void PageItem::on_render_finished()
{
    update();
}

void PageItem::on_imageReady(int physicalDpiX, int physicalDpiY, qreal scaleFactor, Poppler::Page::Rotation rotation, bool prefetch, QImage image)
{
    if(m_physicalDpiX != physicalDpiX || m_physicalDpiY != physicalDpiY || !qFuzzyCompare(m_scaleFactor, scaleFactor) || m_rotation != rotation)
    {
        return;
    }

    if(s_invertColors)
    {
        image.invertPixels();
    }

    if(prefetch)
    {
        s_cache.insert(this, new QImage(image), image.byteCount());
    }
    else
    {
        if(!m_render->isCanceled())
        {
            m_image = image;
        }
    }
}

void PageItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
}

void PageItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if(m_rubberBandMode == ModifiersMode && event->modifiers() == Qt::NoModifier)
    {
        // links

        foreach(Poppler::Link* link, m_links)
        {
            if(m_normalizedTransform.mapRect(link->linkArea().normalized()).contains(event->pos()))
            {
                if(link->linkType() == Poppler::Link::Goto)
                {
                    setCursor(Qt::PointingHandCursor);
                    QToolTip::showText(event->screenPos(), tr("Go to page %1.").arg(static_cast< Poppler::LinkGoto* >(link)->destination().pageNumber()));

                    return;
                }
                else if(link->linkType() == Poppler::Link::Browse)
                {
                    setCursor(Qt::PointingHandCursor);
                    QToolTip::showText(event->screenPos(), tr("Open %1.").arg(static_cast< Poppler::LinkBrowse* >(link)->url()));

                    return;
                }
            }
        }

        // annotations

        foreach(Poppler::Annotation* annotation, m_annotations)
        {
            if(m_normalizedTransform.mapRect(annotation->boundary().normalized()).contains(event->pos()))
            {
                setCursor(Qt::PointingHandCursor);
                QToolTip::showText(event->screenPos(), annotation->contents());

                return;
            }
        }

        // form fields

        foreach(Poppler::FormField* formField, m_formFields)
        {
            if(!formField->isReadOnly() && formField->isVisible() && m_normalizedTransform.mapRect(formField->rect().normalized()).contains(event->pos()))
            {
                setCursor(Qt::PointingHandCursor);
                QToolTip::showText(event->screenPos(), tr("Edit form field."));

                return;
            }
        }

        unsetCursor();
        QToolTip::hideText();
    }
}

void PageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
}

void PageItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    // rubber band

    if(m_rubberBandMode == ModifiersMode && (event->modifiers() == s_copyModifiers || event->modifiers() == s_annotateModifiers) && event->button() == Qt::LeftButton)
    {
        setCursor(Qt::CrossCursor);

        if(event->modifiers() == s_copyModifiers)
        {
            m_rubberBandMode = CopyToClipboardMode;
        }
        else if(event->modifiers() == s_annotateModifiers)
        {
            m_rubberBandMode = AddAnnotationMode;
        }
    }

    if(m_rubberBandMode != ModifiersMode)
    {
        m_rubberBand = QRectF(event->pos(), QSizeF());

        emit rubberBandStarted();

        update();

        event->accept();
        return;
    }

    if(event->modifiers() == Qt::NoModifier && event->button() == Qt::LeftButton)
    {
        // links

        foreach(Poppler::Link* link, m_links)
        {
            if(m_normalizedTransform.mapRect(link->linkArea().normalized()).contains(event->pos()))
            {
                unsetCursor();

                if(link->linkType() == Poppler::Link::Goto)
                {
                    Poppler::LinkGoto* linkGoto = static_cast< Poppler::LinkGoto* >(link);

                    int page = linkGoto->destination().pageNumber();
                    qreal left = linkGoto->destination().isChangeLeft() ? linkGoto->destination().left() : 0.0;
                    qreal top = linkGoto->destination().isChangeTop() ? linkGoto->destination().top() : 0.0;

                    emit linkClicked(page, left, top);

                    event->accept();
                    return;
                }
                else if(link->linkType() == Poppler::Link::Browse)
                {
                    emit linkClicked(static_cast< Poppler::LinkBrowse* >(link)->url());

                    event->accept();
                    return;
                }
            }
        }

        // annotations

        foreach(Poppler::Annotation* annotation, m_annotations)
        {
            if(m_normalizedTransform.mapRect(annotation->boundary().normalized()).contains(event->pos()))
            {
                unsetCursor();

                editAnnotation(annotation, event->screenPos());

                event->accept();
                return;
            }
        }

        // form fields

        foreach(Poppler::FormField* formField, m_formFields)
        {
            if(!formField->isReadOnly() && formField->isVisible() && m_normalizedTransform.mapRect(formField->rect().normalized()).contains(event->pos()))
            {
                unsetCursor();

                editFormField(formField, event->screenPos());

                event->accept();
                return;
            }
        }
    }

    event->ignore();
}

void PageItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    emit sourceRequested(m_index + 1, m_transform.inverted().map(event->pos()));
}

void PageItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if(!m_rubberBand.isNull())
    {
        if(m_boundingRect.contains(event->pos()))
        {
            m_rubberBand.setBottomRight(event->pos());

            update();
        }
    }
}

void PageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if(!m_rubberBand.isNull())
    {
        unsetCursor();

        m_rubberBand = m_rubberBand.normalized();

        if(m_rubberBandMode == CopyToClipboardMode)
        {
            copyToClipboard(event->screenPos());
        }
        else if(m_rubberBandMode == AddAnnotationMode)
        {
            addAnnotation(event->screenPos());
        }

        m_rubberBandMode = ModifiersMode;
        m_rubberBand = QRectF();

        emit rubberBandFinished();

        update();

        event->accept();
        return;
    }

    event->ignore();
}

void PageItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    foreach(Poppler::Annotation* annotation, m_annotations)
    {
        if(m_normalizedTransform.mapRect(annotation->boundary().normalized()).contains(event->pos()))
        {
            unsetCursor();

            removeAnnotation(annotation, event->screenPos());

            event->accept();
            return;
        }
    }

    event->ignore();
}

void PageItem::copyToClipboard(const QPoint& screenPos)
{
    QMenu* menu = new QMenu();

    QAction* copyTextAction = menu->addAction(tr("Copy &text"));
    QAction* copyImageAction = menu->addAction(tr("Copy &image"));
    QAction* saveImageToFileAction = menu->addAction(tr("Save image to &file..."));

    QAction* action = menu->exec(screenPos);

    if(action == copyTextAction)
    {
        QString text;

        m_mutex->lock();

        text = m_page->text(m_transform.inverted().mapRect(m_rubberBand));

        m_mutex->unlock();

        if(!text.isEmpty())
        {
            QApplication::clipboard()->setText(text);
        }
    }
    else if(action == copyImageAction || action == saveImageToFileAction)
    {
        QImage image;

        m_mutex->lock();

        switch(m_rotation)
        {
        case Poppler::Page::Rotate0:
        case Poppler::Page::Rotate90:
            image = m_page->renderToImage(m_scaleFactor * m_physicalDpiX, m_scaleFactor * m_physicalDpiY, m_rubberBand.x(), m_rubberBand.y(), m_rubberBand.width(), m_rubberBand.height(), m_rotation);
            break;
        case Poppler::Page::Rotate180:
        case Poppler::Page::Rotate270:
            image = m_page->renderToImage(m_scaleFactor * m_physicalDpiY, m_scaleFactor * m_physicalDpiX, m_rubberBand.x(), m_rubberBand.y(), m_rubberBand.width(), m_rubberBand.height(), m_rotation);
            break;
        }

        m_mutex->unlock();

        if(!image.isNull())
        {
            if(action == copyImageAction)
            {
                QApplication::clipboard()->setImage(image);
            }
            else if(action == saveImageToFileAction)
            {
                QString fileName = QFileDialog::getSaveFileName(0, tr("Save image to file"), QDir::homePath(), "Portable network graphics (*.png)");

                if(!image.save(fileName, "PNG"))
                {
                    QMessageBox::warning(0, tr("Warning"), tr("Could not save image to file '%1'.").arg(fileName));
                }
            }
        }
    }

    delete menu;
}

void PageItem::addAnnotation(const QPoint& screenPos)
{
#ifdef HAS_POPPLER_20

    QMenu* menu = new QMenu();

    QAction* addTextAction = menu->addAction(tr("Add &text"));
    QAction* addHighlightAction = menu->addAction(tr("Add &highlight"));

    QAction* action = menu->exec(screenPos);

    if(action != 0)
    {
        QRectF boundary = m_normalizedTransform.inverted().mapRect(m_rubberBand);

        Poppler::Annotation::Style style;
        style.setColor(Qt::yellow);

        Poppler::Annotation::Popup popup;
        popup.setFlags(Poppler::Annotation::Hidden | Poppler::Annotation::ToggleHidingOnMouse);

        Poppler::Annotation* annotation = 0;

        if(action == addTextAction)
        {
            boundary.setWidth(24.0 / m_size.width());
            boundary.setHeight(24.0 / m_size.height());

            annotation = new Poppler::TextAnnotation(Poppler::TextAnnotation::Linked);
        }
        else if(action == addHighlightAction)
        {
            Poppler::HighlightAnnotation* highlightAnnotation = new Poppler::HighlightAnnotation();

            Poppler::HighlightAnnotation::Quad quad;
            quad.points[0] = boundary.topLeft();
            quad.points[1] = boundary.topRight();
            quad.points[2] = boundary.bottomRight();
            quad.points[3] = boundary.bottomLeft();

            highlightAnnotation->setHighlightQuads(QList< Poppler::HighlightAnnotation::Quad >() << quad);

            annotation = highlightAnnotation;
        }

        annotation->setBoundary(boundary);
        annotation->setStyle(style);
        annotation->setPopup(popup);

        m_mutex->lock();

        m_annotations.append(annotation);
        m_page->addAnnotation(annotation);

        m_mutex->unlock();

        refresh();

        editAnnotation(annotation, screenPos);
    }

    delete menu;

#else

    Q_UNUSED(screenPos);

#endif // HAS_POPPLER_20
}

void PageItem::removeAnnotation(Poppler::Annotation* annotation, const QPoint& screenPos)
{
#ifdef HAS_POPPLER_20

    QMenu* menu = new QMenu();

    QAction* removeAnnotationAction = menu->addAction(tr("&Remove annotation"));

    QAction* action = menu->exec(screenPos);

    if(action == removeAnnotationAction)
    {
        m_mutex->lock();

        m_annotations.removeAll(annotation);
        m_page->removeAnnotation(annotation);

        m_mutex->unlock();

        refresh();
    }

    delete menu;

#else

    Q_UNUSED(annotation);
    Q_UNUSED(screenPos);

#endif // HAS_POPPLER_20
}

void PageItem::editAnnotation(Poppler::Annotation* annotation, const QPoint& screenPos)
{
    AnnotationDialog* annotationDialog = new AnnotationDialog(m_mutex, annotation);

    annotationDialog->move(screenPos);

    annotationDialog->setAttribute(Qt::WA_DeleteOnClose);
    annotationDialog->show();
}

void PageItem::editFormField(Poppler::FormField *formField, const QPoint &screenPos)
{
    if(formField->type() == Poppler::FormField::FormText || formField->type() == Poppler::FormField::FormChoice)
    {
        FormFieldDialog* formFieldDialog = new FormFieldDialog(m_mutex, formField);

        formFieldDialog->move(screenPos);

        formFieldDialog->setAttribute(Qt::WA_DeleteOnClose);
        formFieldDialog->show();

        connect(formFieldDialog, SIGNAL(destroyed()), SLOT(refresh()));
    }
    else if(formField->type() == Poppler::FormField::FormButton)
    {
        Poppler::FormFieldButton* formFieldButton = static_cast< Poppler::FormFieldButton* >(formField);

        formFieldButton->setState(!formFieldButton->state());

        refresh();
    }
}

void PageItem::prepareGeometry()
{
    m_transform.reset();
    m_normalizedTransform.reset();

    switch(m_rotation)
    {
    case Poppler::Page::Rotate0:
        break;
    case Poppler::Page::Rotate90:
        m_transform.rotate(90.0);
        m_normalizedTransform.rotate(90.0);
        break;
    case Poppler::Page::Rotate180:
        m_transform.rotate(180.0);
        m_normalizedTransform.rotate(180.0);
        break;
    case Poppler::Page::Rotate270:
        m_transform.rotate(270.0);
        m_normalizedTransform.rotate(270.0);
        break;
    }

    switch(m_rotation)
    {
    case Poppler::Page::Rotate0:
    case Poppler::Page::Rotate90:
        m_transform.scale(m_scaleFactor * m_physicalDpiX / 72.0, m_scaleFactor * m_physicalDpiY / 72.0);
        m_normalizedTransform.scale(m_scaleFactor * m_physicalDpiX / 72.0 * m_size.width(), m_scaleFactor * m_physicalDpiY / 72.0 * m_size.height());
        break;
    case Poppler::Page::Rotate180:
    case Poppler::Page::Rotate270:
        m_transform.scale(m_scaleFactor * m_physicalDpiY / 72.0, m_scaleFactor * m_physicalDpiX / 72.0);
        m_normalizedTransform.scale(m_scaleFactor * m_physicalDpiY / 72.0 * m_size.width(), m_scaleFactor * m_physicalDpiX / 72.0 * m_size.height());
        break;
    }

    m_boundingRect = m_transform.mapRect(QRectF(QPointF(), m_size));

    m_boundingRect.setWidth(qRound(m_boundingRect.width()));
    m_boundingRect.setHeight(qRound(m_boundingRect.height()));
}

void PageItem::render(int physicalDpiX, int physicalDpiY, qreal scaleFactor, Poppler::Page::Rotation rotation, bool prefetch)
{
    QMutexLocker mutexLocker(m_mutex);

    if(m_render->isCanceled() && !prefetch)
    {
        return;
    }

    QImage image;

    switch(rotation)
    {
    case Poppler::Page::Rotate0:
    case Poppler::Page::Rotate90:
        image = m_page->renderToImage(scaleFactor * physicalDpiX, scaleFactor * physicalDpiY, -1, -1, -1, -1, rotation);
        break;
    case Poppler::Page::Rotate180:
    case Poppler::Page::Rotate270:
        image = m_page->renderToImage(scaleFactor * physicalDpiY, scaleFactor * physicalDpiX, -1, -1, -1, -1, rotation);
        break;
    }

    if(m_render->isCanceled() && !prefetch)
    {
        return;
    }

    emit imageReady(physicalDpiX, physicalDpiY, scaleFactor, rotation, prefetch, image);
}

ThumbnailItem::ThumbnailItem(QMutex* mutex, Poppler::Page* page, int index, QGraphicsItem* parent) : PageItem(mutex, page, index, parent)
{
    setAcceptHoverEvents(false);
}

void ThumbnailItem::mousePressEvent(QGraphicsSceneMouseEvent*)
{
    emit linkClicked(index() + 1);
}

void ThumbnailItem::mouseMoveEvent(QGraphicsSceneMouseEvent*)
{
}

void ThumbnailItem::mouseReleaseEvent(QGraphicsSceneMouseEvent*)
{
}
