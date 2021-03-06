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
    void pollProgress();
    uint64_t currentDeviceSize();
signals:
    void stateChanged(const State state);
    void finished();
    void progressChanged(unsigned short);
    void errorOccurred(QString msg);
public slots:
    void setDevice(const QString& devName);
    void wipeDevice();
    void createPartition(const unsigned short slot,
                         const QString password);
    void mountPartition(const QString password);
    void unmountPartition();
    void ejectDevice();
    void abortOperation();
private:
    std::string currentDevice;
    State currentState;
    short int mCurrentProgress;
    bool mPollProgress;
    std::unique_ptr<PartitionManager> pm;
    void changeState(State state);
};




#endif // PARTITIONMANAGERADAPTER_H
