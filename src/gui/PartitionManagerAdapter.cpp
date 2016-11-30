#include "gui/PartitionManagerAdapter.h"
#include <QThread>


PartitionManagerAdapter::PartitionManagerAdapter(QObject *parent)
    : QObject(parent),
      currentState(State::NoDeviceSet),
      currentProgress(0)
{
    qRegisterMetaType<State>("State");
}

void PartitionManagerAdapter::setDevice(const QString &devName)
{
    currentDevice = devName.toStdString();
    emit stateChanged(currentState = State::Busy);
    pm.reset(new PartitionManager(currentDevice));
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::wipeDevice()
{
    emit stateChanged(State::Wiping);
    pm->WipeVolume();
    emit finished();
    emit stateChanged(currentState);
}

void PartitionManagerAdapter::createPartition(const unsigned short slot,
                                              const QString &password)
{
    emit stateChanged(currentState = State::Busy);
    pm->CreatePartition(slot,password.toStdString());
    emit finished();
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::mountPartition(const QString &password)
{
    emit stateChanged(currentState = State::Busy);
    pm->MountPartition(password.toStdString());
    emit finished();
    emit stateChanged(currentState = State::PartitionMounted);

}

void PartitionManagerAdapter::unmountPartition()
{
    emit stateChanged(currentState = State::Busy);
    pm->UnmountPartition();
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::ejectDevice()
{
    emit stateChanged(currentState = State::Busy);
    pm.reset();
    emit stateChanged(currentState = State::NoDeviceSet);
}

void PartitionManagerAdapter::abortOperation()
{
    if(pm)
    {
        // pm->abortOperation();
    }
}
