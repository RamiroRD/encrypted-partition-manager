#include "gui/PartitionManagerWindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator;

    translator.load("partmgr_es");
    a.installTranslator(&translator);



    a.setApplicationName(QObject::tr("Partition Manager"));
    a.setApplicationDisplayName(QObject::tr("Partition Manager"));
    PartitionManagerWindow w;
    w.show();

    return a.exec();
}
