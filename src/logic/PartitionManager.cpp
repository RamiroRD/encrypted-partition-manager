
#include <cassert>

#include "logic/PartitionManager.h"



PartitionManager::PartitionManager(const std::string &device)
: mOffsetMultiple(0),
  mPartitionMounted(false),
  mCurrentDevice("")
{

}

void PartitionManager::InitializeVolume()
{
    const std::string cmd = "dd bs=2k if=/dev/urandom of="; 
    
    if(mPartitionMounted)
        throw PartitionStillMountedException();

    assert(mOffsetMultiple != 0);
    
    if(system((cmd + mCurrentDevice).c_str()) == -1)
        throw ExternalErrorException();
}

void PartitionManager::CreatePartition(const off_t offset,
                                       const std::string &password)
{
    if(mPartitionMounted)
        throw PartitionStillMountedException();
    
}

void PartitionManager::MountPartition(const std::string &password)
{
    if(mPartitionMounted)
        throw PartitionStillMountedException();
}

void PartitionManager::UnmountPartition()
{
    if(!mPartitionMounted)
        throw InvalidStateException();
    
}
