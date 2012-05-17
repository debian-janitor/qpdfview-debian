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

#include "mainwindow.h"

struct Link
{
    QString filePath;
    int page;
    qreal top;

    Link() : filePath(), page(1), top(0.0) {}
    Link(const QString& filePath, int page = 1, qreal top = 0.0) : filePath(filePath), page(page), top(top) {}

};

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    QApplication::setOrganizationDomain("local.qpdfview");
    QApplication::setOrganizationName("qpdfview");
    QApplication::setApplicationName("qpdfview");
    QApplication::setApplicationVersion("0.2.99");

#ifdef DATA_INSTALL_PATH

    QApplication::setWindowIcon(QIcon(QString("%1/qpdfview.svg").arg(DATA_INSTALL_PATH)));

    QTranslator t;
    if(t.load(QString("%1/qpdfview_").arg(DATA_INSTALL_PATH) + QLocale::system().name()))
    {
        a.installTranslator(&t);
    }

#else

    QApplication::setWindowIcon(QIcon(":/icons/qpdfview.svg"));

    QTranslator t;
    if(t.load(QString(":/translations/qpdfview_") + QLocale::system().name()))
    {
        a.installTranslator(&t);
    }

#endif

    // command-line arguments

    QStringList arguments = QApplication::arguments();
    QList< Link > links;

    if(!arguments.isEmpty())
    {
        arguments.removeFirst();
    }

    foreach(QString argument, arguments)
    {
        if(!argument.startsWith("--"))
        {
            QStringList fields = argument.split('#');
            Link link;

            link.filePath = fields.at(0);

            if(fields.count() > 1)
            {
                link.page = fields.at(1).toInt();
                link.page = qMax(link.page, 1);
            }
            if(fields.count() > 2)
            {
                link.top = fields.at(2).toFloat();
                link.top = qMax(link.top, 0.0);
                link.top = qMin(link.top, 1.0);
            }

            if(QFileInfo(link.filePath).exists())
            {
                links.append(link);
            }
        }
    }

#ifdef WITH_DBUS

    MainWindow* mainWindow = 0;

    if(arguments.contains("--unique"))
    {
        QDBusInterface interface("local.qpdfview", "/MainWindow", "local.qpdfview.MainWindow", QDBusConnection::sessionBus());

        if(interface.isValid())
        {
            foreach(Link link, links)
            {
                interface.call("refresh", link.filePath, link.page, link.top);
            }

            return 0;
        }
        else
        {
            mainWindow = new MainWindow();

            new MainWindowAdaptor(mainWindow);

            if(!QDBusConnection::sessionBus().registerService("local.qpdfview"))
            {
                qDebug() << QDBusConnection::sessionBus().lastError().message();

                delete mainWindow;
                return 1;
            }

            if(!QDBusConnection::sessionBus().registerObject("/MainWindow", mainWindow))
            {
                qDebug() << QDBusConnection::sessionBus().lastError().message();

                delete mainWindow;
                return 1;
            }
        }
    }
    else
    {
        mainWindow = new MainWindow();
    }

#else

    MainWindow* mainWindow = new MainWindow();

#endif // WITH_DBUS

    foreach(Link link, links)
    {
        mainWindow->openInNewTab(link.filePath, link.page, link.top);
    }

    mainWindow->show();
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    return a.exec();
}
