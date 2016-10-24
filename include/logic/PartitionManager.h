#ifndef PARTITION_MANAGER_H
#define PARTITION_MANAGER_H

#include <exception>
#include <string>

#include <sys/types.h>

// TODO: definir what()
struct NoSuchDeviceException          :  std::exception{};
struct DeviceNotOpenException         :  std::exception{};
struct PermissionDeniedException      :  std::exception{};
struct ExternalErrorException         :  std::exception{};
struct PartitionStillMountedException :  std::exception{};
struct IOExceptionMountedException    :  std::exception{};
struct InvalidStateException          :  std::exception{};

class PartitionManager
{
public:
    PartitionManager(const std::string &device);
    ~PartitionManager();
    
    void InitializeVolume();
    void CreatePartition(const off_t offset, const std::string &password);
    void MountPartition (const std::string &password);
    void UnmountPartition();
private:
    off_t mOffsetMultiple;
    bool mPartitionMounted;
    std::string mCurrentDevice;
    const unsigned PARTITIONS_AMOUNT = 8192;
};
#endif