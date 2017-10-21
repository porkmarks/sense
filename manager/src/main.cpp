#include "Manager.h"
#include <QtWidgets/QApplication>
#include <QNetworkProxyFactory>
#include <QLocale>
#include <QLockFile>

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

    QString lockFileName = (s_dataFolder + "/lock").c_str();
    QLockFile lockFile(lockFileName);
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(1000))
    {
        QLockFile::LockError error = lockFile.error();
        if (error == QLockFile::PermissionError)
        {
            QMessageBox::critical(nullptr, "Permission Error", QString("Cannot create the data lock file '%1' due to a permission error.\n"
                                                                       "Make sure you have permission to write in the data folder.").arg(lockFileName));
        }
        else if (error == QLockFile::LockFailedError)
        {
            qint64 pid = 0;
            QString hostname;
            QString appname;
            QString appinfo = "N/A";
            bool hasInfo = lockFile.getLockInfo(&pid, &hostname, &appname);
            if (hasInfo)
            {
                appinfo = QString("PID: %1, Hostname: %2, Application: %3").arg(pid).arg(hostname).arg(appname);
            }
            QMessageBox::critical(nullptr, "Data Locked", QString("There is another instance of this program running "
                                                                  "over the same data. Close that one first before trying to open "
                                                                  "the program again.\n"
                                                                  "Locking Application Info: %1").arg(appinfo));
        }
        else
        {
            QMessageBox::critical(nullptr, "Lock File Error", QString("There was an error while trying to create the data lock file '%1'.").arg(lockFileName));
        }
        exit(1);
    }

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
