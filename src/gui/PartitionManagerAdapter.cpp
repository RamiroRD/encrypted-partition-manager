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
    pm.reset(nullptr);
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::wipeDevice()
{
    emit stateChanged(State::Wiping);
    QThread::sleep(5);
    emit finished();
    emit stateChanged(currentState);
}

void PartitionManagerAdapter::createPartition()
{
    emit stateChanged(currentState = State::Busy);
    QThread::sleep(3);
    emit finished();
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::mountPartition()
{
    emit stateChanged(currentState = State::Busy);
    QThread::sleep(3);
    emit finished();
    emit stateChanged(currentState = State::PartitionMounted);

}

void PartitionManagerAdapter::unmountPartition()
{
    emit stateChanged(currentState = State::Busy);
    QThread::sleep(1);
    emit stateChanged(currentState = State::PartitionUnmounted);
}

void PartitionManagerAdapter::ejectDevice()
{
    emit stateChanged(currentState = State::Busy);
    QThread::sleep(1);
    emit stateChanged(currentState = State::NoDeviceSet);
}
