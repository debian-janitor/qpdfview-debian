/*

Copyright 2012 Adam Reichold

This file is part of qpdfview.

qpdfview is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

qpdfview is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with qpdfview.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DOCUMENTVIEW_H
#define DOCUMENTVIEW_H

#include <QtCore>
#include <QtXml>
#include <QtGui>

#include <poppler-qt4.h>

#ifdef WITH_CUPS

#include <cups/cups.h>

#endif

#include "miscellaneous.h"

class DocumentView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(int numberOfPages READ numberOfPages NOTIFY numberOfPagesChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(PageLayout pageLayout READ pageLayout WRITE setPageLayout NOTIFY pageLayoutChanged)
    Q_PROPERTY(ScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(Rotation rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool highlightAll READ highlightAll WRITE setHighlightAll NOTIFY highlightAllChanged)
    Q_ENUMS(PageLayout ScaleMode Rotation)

public:
    // enums

    enum PageLayout { OnePage, TwoPages, OneColumn, TwoColumns };
    enum ScaleMode { FitToPage, FitToPageWidth, DoNotScale, ScaleFactor };
    enum Rotation { RotateBy0, RotateBy90, RotateBy180, RotateBy270 };

    // static settings

    static const qreal pageSpacing;
    static const qreal thumbnailSpacing;

    static qreal thumbnailWidth;
    static qreal thumbnailHeight;

    static const qreal zoomBy;

    static const qreal mininumScaleFactor;
    static const qreal maximumScaleFactor;

    static bool fitToEqualWidth;

    static bool highlightLinks;
    static bool externalLinks;

private:
    struct Link
    {
        QRectF area;

        int page;
        qreal top;

        QString url;

        Link() : area(), page(-1), top(0.0), url() {}
        Link(QRectF area, int page, qreal top) : area(area), page(page), top(top), url() {}
        Link(QRectF area, const QString& url) : area(area), page(-1), top(0.0), url(url) {}

    };

    struct PageCacheKey
    {
        int index;
        qreal resolutionX;
        qreal resolutionY;

        PageCacheKey() : index(-1), resolutionX(72.0), resolutionY(72.0) {}
        PageCacheKey(int index, qreal resolutionX, qreal resolutionY) : index(index), resolutionX(resolutionX), resolutionY(resolutionY) {}

        bool operator<(const PageCacheKey& key) const
        {
            return (index < key.index) ||
                   (index == key.index && !qFuzzyCompare(resolutionX, key.resolutionX) && resolutionX < key.resolutionX) ||
                   (index == key.index && qFuzzyCompare(resolutionX, key.resolutionX) && !qFuzzyCompare(resolutionY, key.resolutionY) && resolutionY < key.resolutionY);
        }

    };

    struct PageCacheValue
    {
        QTime time;
        QImage image;

        PageCacheValue() : time(), image() {}
        PageCacheValue(const QImage& image) : time(), image(image) {}

    };

    friend class PageItem;

    class PageItem : public QGraphicsItem
    {
        friend class DocumentView;

    public:
        PageItem(QGraphicsItem* parent = 0, QGraphicsScene* scene = 0);
        ~PageItem();

        QRectF boundingRect() const;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*);

    protected:
        void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
        void hoverMoveEvent(QGraphicsSceneHoverEvent* event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
        void mousePressEvent(QGraphicsSceneMouseEvent* event);
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

    private:
        // page

        Poppler::Page* m_page;

        // properties

        int m_index;
        qreal m_scale;

        // links

        QList< DocumentView::Link > m_links;

        // selections

        QRectF m_highlight;
        QRectF m_rubberBand;

        // transforms

        QSizeF m_size;

        QTransform m_linkTransform;
        QTransform m_highlightTransform;

        // render

        QFuture< void > m_render;
        void render(qreal scale, bool prefetch = false);

    };

    friend class ThumbnailItem;

    class ThumbnailItem : public QGraphicsItem
    {
        friend class DocumentView;

    public:
        ThumbnailItem(QGraphicsItem* parent = 0, QGraphicsScene* scene = 0);
        ~ThumbnailItem();

        QRectF boundingRect() const;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*);

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);

    private:
        // page

        Poppler::Page* m_page;

        // properties

        int m_index;
        qreal m_scale;

        // transforms

        QSizeF m_size;

        // render

        QFuture< void > m_render;
        void render(qreal scale);

    };

public:
    explicit DocumentView(QWidget* parent = 0);
    ~DocumentView();

    // properties

    const QString& filePath() const;
    int numberOfPages() const;

    int currentPage() const;

    PageLayout pageLayout() const;
    void setPageLayout(PageLayout pageLayout);

    ScaleMode scaleMode() const;
    void setScaleMode(ScaleMode scaleMode);

    qreal scaleFactor() const;
    void setScaleFactor(qreal scaleFactor);

    Rotation rotation() const;
    void setRotation(Rotation rotation);

    bool highlightAll() const;
    void setHighlightAll(bool highlightAll);

    QAction* tabAction() const;

    QTreeWidget* outlineTreeWidget() const;
    QTableWidget* metaInformationTableWidget() const;
    QGraphicsView* thumbnailsGraphicsView() const;

    QTableWidget* fontsTableWidget();

public slots:
    bool open(const QString& filePath);
    bool refresh();
    bool saveCopy(const QString& filePath);

    void setCurrentPage(int currentPage, qreal top = 0.0);

    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();

    void zoomIn();
    void zoomOut();

    void rotateLeft();
    void rotateRight();

    void startSearch(const QString& text, bool matchCase = true);
    void cancelSearch();

    void findPrevious();
    void findNext();

    void startPrint(QPrinter* printer, int fromPage, int toPage);
    void cancelPrint();

signals:
    void pageItemChanged(PageItem* pageItem);
    void thumbnailItemChanged(ThumbnailItem* thumbnailItem);

    void filePathChanged(const QString& filePath);
    void numberOfPagesChanged(int numberOfPages);

    void currentPageChanged(int currentPage);
    void pageLayoutChanged(DocumentView::PageLayout pageLayout);
    void scaleModeChanged(DocumentView::ScaleMode scaleMode);
    void scaleFactorChanged(qreal scaleFactor);
    void rotationChanged(DocumentView::Rotation rotation);

    void highlightAllChanged(bool highlightAll);

    void searchProgressed(int value);
    void searchCanceled();
    void searchFinished();

    void firstResultFound();

    void printProgressed(int value);
    void printCanceled();
    void printFinished();

protected:
    bool eventFilter(QObject* object, QEvent* event);

    void showEvent(QShowEvent* event);
    void resizeEvent(QResizeEvent* event);

    void contextMenuEvent(QContextMenuEvent* event);

    void keyPressEvent(QKeyEvent* event);
    void wheelEvent(QWheelEvent* event);

protected slots:
    void slotUpdatePageItem(PageItem* pageItem);
    void slotUpdateThumbnailItem(ThumbnailItem* thumbnailItem);

    void slotVerticalScrollBarValueChanged(int value);

    void slotPrefetchTimerTimeout();
    void slotBookmarksMenuEntrySelected(int page, int value);
    void slotTabActionTriggered();
    void slotOutlineTreeWidgetItemClicked(QTreeWidgetItem* item, int column);

private:
    // document

    Poppler::Document* m_document;
    QMutex m_documentMutex;

    // page cache

    QMap< PageCacheKey, PageCacheValue > m_pageCache;
    QMutex m_pageCacheMutex;

    uint m_pageCacheSize;
    uint m_maximumPageCacheSize;

    // properties

    QString m_filePath;
    int m_numberOfPages;

    int m_currentPage;
    PageLayout m_pageLayout;
    ScaleMode m_scaleMode;
    qreal m_scaleFactor;
    Rotation m_rotation;

    bool m_highlightAll;

    // settings

    QSettings m_settings;

    // graphics

    QGraphicsScene* m_scene;
    QGraphicsView* m_view;

    QGraphicsRectItem* m_highlight;

    QVector< PageItem* > m_pagesByIndex;
    QMap< qreal, PageItem* > m_pagesByHeight;

    qreal m_resolutionX;
    qreal m_resolutionY;

    QTransform m_pageTransform;

    // miscellaneous

    QFileSystemWatcher* m_autoRefreshWatcher;
    QTimer* m_autoRefreshTimer;
    QTimer* m_prefetchTimer;
    BookmarksMenu* m_bookmarksMenu;
    QAction* m_tabAction;
    QTreeWidget* m_outlineTreeWidget;
    QTableWidget* m_metaInformationTableWidget;
    QGraphicsView* m_thumbnailsGraphicsView;

    // search

    QMap< int, QRectF > m_results;
    QMutex m_resultsMutex;

    QMap< int, QRectF >::const_iterator m_currentResult;

    QFuture< void > m_search;
    void search(const QString& text, bool matchCase = true);

    // print

    QFuture< void > m_print;
    void print(QPrinter* printer, int fromPage, int toPage);

    // internal methods

    void clearScene();
    void clearPageCache();

    void updatePageCache(const PageCacheKey& key, const PageCacheValue& value);

    void preparePages();

    void prepareOutline();
    void prepareOutline(const QDomNode& node, QTreeWidgetItem* parent, QTreeWidgetItem* sibling);

    void prepareMetaInformation();

    void prepareThumbnails();

    void prepareScene();
    void prepareView(qreal top = 0.0);
    
};

#endif // DOCUMENTVIEW_H
