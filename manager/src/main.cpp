#include "Manager.h"
#include <QtWidgets/QApplication>
#include <QNetworkProxyFactory>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(res);

    QApplication a(argc, argv);
//    QApplication::setStyle("fusion");

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QCoreApplication::setOrganizationName("Sense");
    QCoreApplication::setOrganizationDomain("sense.com");
    QCoreApplication::setApplicationName("Sense");

    a.setQuitOnLastWindowClosed(true);

    Manager w;
    w.show();
    auto res = a.exec();

    return res;
}
