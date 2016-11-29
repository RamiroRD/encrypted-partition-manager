#ifndef PARTITIONMANAGERADAPTER_H
#define PARTITIONMANAGERADAPTER_H

#include <memory>

#include <QObject>

#include <include/logic/PartitionManager.h>



enum class State
{

    NoDeviceSet,
    PartitionMounted,
    PartitionUnmounted,
    Wiping,
    Unmounting,
    Mounting,
    Busy
};
Q_DECLARE_METATYPE(State)


/*
 * Esta clase se encarga únicamente de hacer que las llamadas a métodos de
 * PartitionManager se hagan en otro thread.
 * Esta clase tiene que "residir" en otro thread y sus métodos se llaman usando
 * signals y slots.
 */
class PartitionManagerAdapter : public QObject
{
    Q_OBJECT
public:
    explicit PartitionManagerAdapter(QObject *parent = 0);

signals:
    void stateChanged(const State state);
    void finished();
    void progressChanged(unsigned short);
public slots:
    void setDevice(const QString& devName);
    void wipeDevice();
    void createPartition();
    void mountPartition();
    void unmountPartition();
    void ejectDevice();
private:
    std::string currentDevice;
    State currentState;
    short int currentProgress;
    std::unique_ptr<PartitionManager> pm;
};




#endif // PARTITIONMANAGERADAPTER_H
