TARGET = qpdfview
TEMPLATE = app

QT       += core gui

INCLUDEPATH  += /usr/include/poppler/qt4
LIBS         += -L/usr/lib -lpoppler-qt4

SOURCES += main.cpp mainwindow.cpp \
    documentview.cpp \
    pageobject.cpp

HEADERS  += mainwindow.h \
    documentview.h \
    pageobject.h

RESOURCES += qpdfview.qrc

ICON = icons/qpdfview.svg

TRANSLATIONS += translations/qpdfview_de.ts

target.path = /usr/bin

launcher.path = /usr/share/applications
launcher.files = miscellaneous/qpdfview.desktop

pixmap.path = /usr/share/pixmaps
pixmap.files = icons/qpdfview.png

manpage.path = /usr/man/man1
manpage.files = miscellaneous/qpdfview.1

INSTALLS += target launcher pixmap manpage
