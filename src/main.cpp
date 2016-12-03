#include <unistd.h>
#include <sys/types.h>

#include <QApplication>
#include <QTranslator>
#include <QMessageBox>


#include "gui/PartitionManagerWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator;

    translator.load("partmgr_es");
    a.installTranslator(&translator);



    a.setApplicationName(QObject::tr("Partition Manager"));
    a.setApplicationDisplayName(QObject::tr("Partition Manager"));

    if(getuid()==0)
    {
        PartitionManagerWindow w;
        w.show();
        return a.exec();
    }else{
        QMessageBox::warning(nullptr,
                             QObject::tr("Superuser privileges required."),
                             QObject::tr("This program needs superuser privileges to access storage devices. Please run this program again as root."));
        return -1;
    }


}
