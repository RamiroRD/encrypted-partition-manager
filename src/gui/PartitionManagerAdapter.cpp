#include "gui/PartitionManagerAdapter.h"
#include <QThread>
#include <thread>
#include <iostream>

#include <unistd.h>


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
    if(pm->isPartitionMounted())
        emit stateChanged(currentState = State::PartitionMounted);
    else
        emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::wipeDevice()
{
    emit stateChanged(State::Wiping);
    pm->resetProgress();
    std::thread thr(&PartitionManagerAdapter::pollProgress,this);
    pm->wipeDevice();
    emit finished();
    thr.join();
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::pollProgress()
{
    currentProgress = pm->getProgress();
    while(currentProgress<100)
    {
        emit progressChanged(currentProgress);
        currentProgress = pm->getProgress();
        usleep(20000);
    }

}

void PartitionManagerAdapter::createPartition(const unsigned short slot,
                                              const QString password)
{
    emit stateChanged(currentState = State::Busy);
    bool result = pm->createPartition(slot, password.toStdString());
    emit finished();
    if(result)
        emit stateChanged(currentState = State::PartitionMounted);
    else
        emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::mountPartition(const QString password)
{
    emit stateChanged(currentState = State::Busy);
    bool result = pm->mountPartition(password.toStdString());
    emit finished();
    if(result)
        emit stateChanged(currentState = State::PartitionMounted);
    else
        emit stateChanged(currentState = State::PartitionUnmounted);

}

void PartitionManagerAdapter::unmountPartition()
{
    emit stateChanged(currentState = State::Busy);
    pm->unmountPartition();
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::ejectDevice()
{
    emit stateChanged(currentState = State::Busy);
    pm->ejectDevice();
    pm.reset(nullptr);
    emit stateChanged(currentState = State::NoDeviceSet);
}

void PartitionManagerAdapter::abortOperation()
{
    if(pm)
        pm->abortOperation();
}
