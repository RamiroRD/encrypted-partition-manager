#ifndef PARTITION_MANAGER_H
#define PARTITION_MANAGER_H

#include <sys/types.h>

#include <stdexcept>
#include <string>
#include <atomic>



struct CommandError : public std::runtime_error
{
    CommandError(const std::string &cmd) : std::runtime_error(cmd){};
    CommandError(const char* &cmd) : std::runtime_error(cmd){};
};
struct SysCallError : public std::exception
{
    
    SysCallError(const char * cmd, int exitCode)
    {
        mWhat += "Syscall: \"";
        mWhat += cmd;
        mWhat += "\". Codigo de retorno: ";
        mWhat += std::to_string(exitCode);
    }
    SysCallError(const std::string &cmd, int exitCode)
    {
        SysCallError(cmd.c_str(),exitCode);
    }

    const char * what() const noexcept
    {
        return mWhat.c_str();
    }
    private:
    std::string mWhat;
};
struct PartitionNotFoundException : public std::exception{};

class PartitionManager
{
public:
    PartitionManager(const std::string &device);
    ~PartitionManager();
    
    void WipeVolume();
    void CreatePartition(const unsigned short slot,
                         const std::string &password);
    void MountPartition (const std::string &password);
    void UnmountPartition();
    unsigned char getProgress() const;
    static constexpr unsigned short SLOTS_AMOUNT = 8192;
private:
    // Ambos en bloques 
    off_t mDeviceSize;
    off_t mOffsetMultiple;
    std::string mCurrentDevice;
    std::atomic<unsigned char> mProgress;
    void closeMapping(const std::string&);
    void openCryptMapping(const unsigned short slot,
                          const std::string &password);
    void createWraparound();
    bool logicalDeviceExists(const std::string&);
    bool isMountPoint(const std::string& dirPath);
    static constexpr unsigned short BLOCK_SIZE_ = 512;
    
    // Workaround al bug 54483 de gcc.
    const off_t MAX_TRANSFER_SIZE = 1000;
    static constexpr const char * WRAPAROUND_DEVICE_NAME = "wraparound";
    static constexpr const char * MAPPINGS_FOLDER_PATH = "/dev/mapper/";
    static constexpr const char * ENCRYPTED_DEVICE_NAME = "encrypted";
    static constexpr const char * MOUNT_POINT = "/mnt/part";
    
};
#endif