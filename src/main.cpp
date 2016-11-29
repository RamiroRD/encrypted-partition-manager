#include "gui/PartitionManagerWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QObject::tr("Partition Manager"));
    a.setApplicationDisplayName(QObject::tr("Partition Manager"));
    PartitionManagerWindow w;
    w.show();

    return a.exec();
}
