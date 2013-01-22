/*

Copyright 2012-2013 Adam Reichold

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

#ifndef PRESENTATIONVIEW_H
#define PRESENTATIONVIEW_H

#include <QFutureWatcher>
#include <QStack>
#include <QGraphicsView>

#include "global.h"

namespace Model
{
class Document;
}

class PageItem;

class PresentationView : public QGraphicsView
{
    Q_OBJECT

public:
    PresentationView(Model::Document* document, QWidget* parent = 0);
    ~PresentationView();

    int numberOfPages() const;
    int currentPage() const;

signals:
    void currentPageChanged(int currentPage, bool returnTo = false);

    void rotationChanged(Rotation rotation);
    
public slots:
    void show();

    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();

    void jumpToPage(int page, bool returnTo = true);

    void rotateLeft();
    void rotateRight();

protected slots:
    void on_prefetch_timeout();

    void on_pages_linkClicked(int page, qreal left, qreal top);

protected:
    void resizeEvent(QResizeEvent* event);

    void keyPressEvent(QKeyEvent* event);
    void wheelEvent(QWheelEvent* event);

private:
    QTimer* m_prefetchTimer;

    Model::Document* m_document;

    int m_numberOfPages;
    int m_currentPage;

    Rotation m_rotation;

    QStack< int > m_returnToPage;

    QGraphicsScene* m_pagesScene;
    QVector< PageItem* > m_pages;

    void prepareScene();
    void prepareView();
    
};

#endif // PRESENTATIONVIEW_H
