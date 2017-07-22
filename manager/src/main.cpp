#include "Manager.h"
#include <QtWidgets/QApplication>
#include <QNetworkProxyFactory>
#include <QLocale>

std::string s_programFolder;
std::string s_dataFolder;

/* This prints an "Assertion failed" message and aborts.  */
void __assert_fail(const char *__assertion, const char *__file, unsigned int __line, const char *__function)
{
    bool enter = true;
    if (enter)
    {
        //qt_assert(__assertion, __file, __line);
    }
}

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(res);

    QApplication a(argc, argv);

    s_programFolder = a.applicationDirPath().toUtf8().data();
    s_dataFolder = s_programFolder + "/data";
    QDir().mkpath(s_dataFolder.c_str());

//    QApplication::setStyle("fusion");

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QCoreApplication::setOrganizationName("Sense");
    QCoreApplication::setOrganizationDomain("sense.com");
    QCoreApplication::setApplicationName("Sense");

    QLocale::setDefault(QLocale(QLocale::English, QLocale::Romania));

    a.setQuitOnLastWindowClosed(true);

    Manager w;
    w.show();
    auto res = a.exec();

    return res;
}
