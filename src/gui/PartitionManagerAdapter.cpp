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
    changeState(State::Wiping);
    pm->resetProgress();
    std::thread thr(&PartitionManagerAdapter::pollProgress,this);
    pm->wipeDevice();
    emit finished();
    thr.join();
    changeState(State::PartitionUnmounted);
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
    changeState(State::Busy);

    try
    {
        try
        {

            pm->createPartition(slot, password.toStdString());
            changeState(State::PartitionMounted);
        }catch(CommandError &e){
            emit errorOccurred(tr("An external command failed. Device will be ejected."));
            throw e;
        }catch(SysCallError &e){
            QString errorMsg = tr("System call: \"");
            errorMsg += QString(e.sysCall().c_str());
            errorMsg += "\" failed with return code ";
            errorMsg += QString::number(e.returnCode());
            errorMsg += ".";

            emit errorOccurred(errorMsg);
            throw e;
        }
    }catch(std::exception &e){
        pm->ejectDevice();
        pm.reset(nullptr);
        std::cerr << e.what() << std::endl;
        changeState(State::NoDeviceSet);
    }


    emit finished();


}

void PartitionManagerAdapter::mountPartition(const QString password)
{
    try
    {
        try
        {
            changeState(State::Busy);
            if(pm->mountPartition(password.toStdString()))
                changeState(State::PartitionMounted);
            else
                changeState(State::PartitionUnmounted);

        }catch(std::domain_error &e){
            emit errorOccurred(tr("Empty password."));
            changeState(State::PartitionUnmounted);
        }catch(CommandError &e){
            emit errorOccurred(tr("An external command failed. Device will be ejected."));
            throw e;
        }catch(SysCallError &e){
            QString errorMsg = tr("System call: \"");
            errorMsg += QString(e.sysCall().c_str());
            errorMsg += "\" failed with return code ";
            errorMsg += QString::number(e.returnCode());
            errorMsg += ".";

            emit errorOccurred(errorMsg);
            throw e;
        }
    }catch(std::exception &e){
        pm->ejectDevice();
        pm.reset(nullptr);
        std::cerr << e.what() << std::endl;
        changeState(State::NoDeviceSet);
    }
    emit finished();
}

void PartitionManagerAdapter::unmountPartition()
{
    changeState(State::Busy);
    pm->unmountPartition();
    changeState(State::PartitionUnmounted);
}

void PartitionManagerAdapter::ejectDevice()
{
    changeState(State::Busy);
    pm->ejectDevice();
    pm.reset(nullptr);
    changeState(State::NoDeviceSet);
}

void PartitionManagerAdapter::abortOperation()
{
    if(pm)
        pm->abortOperation();
}

void PartitionManagerAdapter::changeState(State state)
{
    emit stateChanged(currentState=state);
}
