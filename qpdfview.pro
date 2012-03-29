include(qpdfview.pri)

TARGET = qpdfview
TEMPLATE = app

QT += core xml gui

# uncomment to append the poppler headers and libraries manually
#
#INCLUDEPATH += /usr/include/poppler/qt4
#LIBS += -L/usr/lib -lpoppler-qt4

# uncomment to use pkgconfig to find the poppler headers and libraries
#
CONFIG += link_pkgconfig
PKGCONFIG += poppler-qt4

SOURCES += \
    sources/document.cpp \
    sources/pageview.cpp \
    sources/documentview.cpp \
    sources/miscellaneous.cpp \
    sources/mainwindow.cpp \
    sources/main.cpp

HEADERS += \
    sources/document.h \
    sources/pageview.h \
    sources/documentview.h \
    sources/miscellaneous.h \
    sources/mainwindow.h

TRANSLATIONS += \
    translations/qpdfview_be.ts \
    translations/qpdfview_de.ts \
    translations/qpdfview_fr.ts \
    translations/qpdfview_ru.ts \
    translations/qpdfview_uk.ts

target.path = $${TARGET_INSTALL_PATH}

launcher.path = $${LAUNCHER_INSTALL_PATH}
launcher.files = miscellaneous/qpdfview.desktop

manual.path = $${MANUAL_INSTALL_PATH}
manual.files = miscellaneous/qpdfview.1

data.path = $${DATA_INSTALL_PATH}

INSTALLS += target launcher manual data

# uncomment to use the Qt resource system for data files
#
#data.files = icons/qpdfview.svg
#
#RESOURCES += qpdfview.qrc

# uncomment to install data files into DATA_INSTALL_PATH
#
data.files = icons/*.svg translations/*.qm miscellaneous/help.html
DEFINES += DATA_INSTALL_PATH=\\\"$${DATA_INSTALL_PATH}\\\"
