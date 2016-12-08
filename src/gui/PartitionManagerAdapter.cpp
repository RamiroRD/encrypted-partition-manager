#include "gui/PartitionManagerAdapter.h"
#include <QThread>
#include <thread>
#include <iostream>

#include <unistd.h>


PartitionManagerAdapter::PartitionManagerAdapter(QObject *parent)
    : QObject(parent),
      currentState(State::NoDeviceSet),
      mCurrentProgress(0),
      mPollProgress(false)
{
    qRegisterMetaType<State>("State");
}

void PartitionManagerAdapter::setDevice(const QString &devName)
{
    currentDevice = devName.toStdString();
    try
    {
        try
        {
            emit stateChanged(currentState = State::Busy);
            pm.reset(new PartitionManager(currentDevice));
            if(pm->isPartitionMounted())
                emit stateChanged(currentState = State::PartitionMounted);
            else
                emit stateChanged(currentState = State::PartitionUnmounted);
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
        emit errorOccurred(tr("Could not open device. It is probably busy or it no longer exists."));
        pm.reset(nullptr);
        std::cerr << e.what() << std::endl;
        changeState(State::NoDeviceSet);
    }
}

void PartitionManagerAdapter::wipeDevice()
{
    try
    {
        changeState(State::Wiping);
        pm->resetProgress();
        mPollProgress = true;
        std::thread thr(&PartitionManagerAdapter::pollProgress,this);
        thr.detach();
        pm->wipeDevice();
        emit finished();
        changeState(State::PartitionUnmounted);
    }catch(std::exception &e){
        pm.reset(nullptr);
        // para que pare el thread solo
        mPollProgress=false;
        std::cerr << e.what() << std::endl;
        changeState(State::NoDeviceSet);
        emit errorOccurred(tr("Device wipe failed."));
    }
}

void PartitionManagerAdapter::pollProgress()
{
    mCurrentProgress = pm->getProgress();
    while(mCurrentProgress<100 && mPollProgress)
    {
        emit progressChanged(mCurrentProgress);
        mCurrentProgress = pm->getProgress();
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
    try
    {
        changeState(State::Busy);
        pm->unmountPartition();
        changeState(State::PartitionUnmounted);
    }catch(SysCallError &e){
        emit errorOccurred(tr("Unmount system called failed."));
    }catch(CommandError &e){
        emit errorOccurred(tr("An external command failed. Device will be ejected."));
    }
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
