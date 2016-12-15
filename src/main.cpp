#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>

#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMessageBox>


#include "gui/PartitionManagerWindow.h"


bool isProcessUnique()
{
    int instanceFile = open("/tmp/pmgr.pid", O_CREAT | O_RDWR, 0666);
    int lock = flock(instanceFile, LOCK_EX | LOCK_NB);
    return !lock;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator qtTranslator;
    qtTranslator.load("qt_es","/usr/share/qt4/translations");
    a.installTranslator(&qtTranslator);

    QTranslator pmTranslator;
    pmTranslator.load("partmgr_es","/usr/bin");
    a.installTranslator(&pmTranslator);


    a.setApplicationName(QObject::tr("Partition Manager"));
    a.setApplicationDisplayName(QObject::tr("Partition Manager"));

    if(getuid()!=0)
    {
        QMessageBox::warning(nullptr,
                             QObject::tr("Superuser privileges required."),
                             QObject::tr("This program needs superuser privileges to access storage devices. Please run this program again as root."));
        return -1;
    }

    if(!isProcessUnique())
    {
        QMessageBox::warning(nullptr,
                             QObject::tr("Another instance is open"),
                             QObject::tr("There is another instance of this program running. Running multiple instances is not supported."));
        return -1;
    }



    PartitionManagerWindow w;
    w.show();
    return a.exec();
}
