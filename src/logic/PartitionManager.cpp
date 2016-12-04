#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <mntent.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <parted/parted.h>


#include "logic/PartitionManager.h"

const std::string PartitionManager::WRAPAROUND = "wraparound";
const std::string PartitionManager::MAPPER_DIR = "/dev/mapper/";
const std::string PartitionManager::ENCRYPTED = "encrypted";
const std::string PartitionManager::MOUNTPOINT = "/mnt/part";


static bool fileExists(const std::string &path)
{
    std::ifstream f(path);
    return f.good();
}

bool PartitionManager::logicalDeviceExists(const std::string &logicalDev)
{
    std::string filePath = PartitionManager::MAPPER_DIR;
    filePath += logicalDev;
    return fileExists(filePath);
}


PartitionManager::PartitionManager(const std::string &device)
    : mDeviceSize(0),
      mOffsetMultiple(0),
      mCurrentDevice(device),
      mProgress(0),
      mOperationCanceled(false),
      mCloseAtDestroy(false)
{
    std::lock_guard<std::mutex> lock(mGuard);
    /*
     * Verificamos que el dispositivo exista. Si no existe, entonces se levanta
     * una excepción que debería manejarse desde el caller del constructor.
     */
     int fd = open(device.c_str(),O_RDWR);
     if(fd == -1)
     {
        if(errno == ENOENT)
            throw std::domain_error("No such device.");
        else if (errno == EPERM || errno == EACCES)
            throw std::runtime_error("Permission denied.");
        else
            throw std::runtime_error("Unknown error when opening device block file.");
     }else{
        unsigned long long size;
        fstat(fd,&mFileStat);

        if(!S_ISBLK(mFileStat.st_mode))
            throw std::domain_error("Not a block file!");
        if(ioctl(fd, BLKGETSIZE64, &size)==-1)
            throw SysCallError("ioctl BLKGETSIZE64",errno);
        
        mDeviceSize = size/BLOCK_SIZE_;
        mOffsetMultiple = mDeviceSize / SLOTS_AMOUNT;
        std::cerr << device << " has " << mDeviceSize << " blocks."
                << std::endl;
     }
    close(fd);



    /*
     * Verificar wraparound y /mnt/part.
     */
    if(fileExists(MAPPER_DIR+WRAPAROUND))
    {
        if(!isWraparoundOurs())
        {
            if(isPartitionMounted())
                unmountPartition();
            if(fileExists(MAPPER_DIR+ENCRYPTED))
                closeMapping(ENCRYPTED);
            closeMapping(MAPPER_DIR+WRAPAROUND);
            createWraparound();
        }
    }
    else
            createWraparound();
}

void PartitionManager::createWraparound()
{
    std::stringstream table;
    table << 0 << " " << mDeviceSize << " linear " << mCurrentDevice << " "
          << 0 << std::endl
          << mDeviceSize << " " << mDeviceSize << " linear " << mCurrentDevice << " "
          << 0 << std::endl;

    FILE *stream = popen("dmsetup create wraparound", "w");

    const std::string &input = table.str();
    fwrite(input.c_str(), 1, input.size(), stream);

    std::cerr << "Opening wraparound ..." << std::endl;
    if (pclose(stream) != 0)
        throw CommandError("dmsetup create wraparound");
}


// dirPath sin la barra!
bool PartitionManager::isMountPoint(const std::string &dirPath)
{
    FILE * mtabFile = setmntent("/etc/mtab","r");
    struct mntent * mountEntry = getmntent(mtabFile);
    std::string encryptedDevicePath = std::string(MAPPER_DIR) +
                                        ENCRYPTED;
    bool found = false;
    for(;mountEntry != nullptr && !found;
        mountEntry=getmntent(mtabFile))
    {
        if(strcmp(mountEntry->mnt_dir,dirPath.c_str())==0)
        {
            found = true;
            if(strcmp(encryptedDevicePath.c_str(),mountEntry->mnt_fsname) !=0)
                throw std::runtime_error("Another device mounted on mountpoint!");
        }
    }

    endmntent(mtabFile);
    return found;
}

void PartitionManager::openCryptMapping(const unsigned short slot,
                                        const std::string &password)
{
    const off_t offset = slot * mOffsetMultiple;
    std::stringstream ss;
    ss << "cryptsetup --hash=sha512 --cipher=aes-xts-plain64";
    ss << " --offset " << offset;
    ss << " open --size " << mDeviceSize;
    ss << " --type=plain " << MAPPER_DIR << WRAPAROUND;
    ss << " " << ENCRYPTED;
    const std::string &cmd = ss.str();

    FILE *stream = popen(cmd.c_str(), "w");
    fwrite(password.c_str(), 1, password.size(), stream);
    fputc('\n', stream);
    if (pclose(stream) != 0)
        throw CommandError(cmd);
}

void PartitionManager::closeMapping(const std::string &logicalDevice)
{
    if (logicalDeviceExists(logicalDevice))
    {
        std::string cmd = "dmsetup remove --force ";
        cmd += logicalDevice;
        system(cmd.c_str());
    }
}

unsigned char PartitionManager::getProgress() const
{
    return mProgress;
}

bool PartitionManager::wipeDevice()
{
    std::lock_guard<std::mutex> lock(mGuard);

    if(isMountPoint(MOUNTPOINT))
        return false;
    if(mProgress != 0)
        return false;

    std::string path = MAPPER_DIR;
    path += WRAPAROUND;

    int in  = open("/dev/urandom", O_RDONLY);
    int out = open(path.c_str(), O_WRONLY);

    std::cerr << "Wiping device ..." << std::endl;

    const auto deviceSize = mDeviceSize * BLOCK_SIZE;
    std::vector<char> buffer(MAX_TRANSFER_SIZE,'\0');
    mProgress = 0;
    unsigned amount = std::min(deviceSize, MAX_TRANSFER_SIZE);
    for (volatile off_t i = 0;
         i < deviceSize && !mOperationCanceled;
         i += (amount = std::min(deviceSize - i, MAX_TRANSFER_SIZE)))
    {
        if(read(in,buffer.data(),amount) == -1)
        {
            std::cerr << "Read failed." << std::endl;
            return false;
        }

        if(write(out,buffer.data(),amount) == -1)
        {
            std::cerr << "Write failed." << std::endl;
            return false;
        }
        mProgress = std::ceil(100 * i / deviceSize);
    }

    if(mOperationCanceled)
    {
        std::cout << "Wipe canceled." << std::endl;
        mOperationCanceled = false;
    }
    mProgress = 100;
    std::cerr << "Device wiped." << std::endl;
    return true;

}

bool PartitionManager::createPartition(const unsigned short slot,
                                       const std::string &password)
{
    std::lock_guard<std::mutex> lock(mGuard);
    if (isMountPoint(MOUNTPOINT))
        throw std::logic_error("Partition is already mounted");
    if (slot >= SLOTS_AMOUNT)
        throw std::domain_error("Slot number out of range");
    if (password.empty())
        throw std::domain_error("Empty password");
    
    closeMapping(ENCRYPTED);
    openCryptMapping(slot,password);

    std::string cmd = "mkfs.vfat ";
    cmd += MAPPER_DIR;
    cmd += ENCRYPTED;
    if(system(cmd.c_str())!=0)
    {
        throw CommandError(cmd);
    }
    return callMount();
}

bool PartitionManager::mountPartition(const std::string &password)
{
    std::lock_guard<std::mutex> lock(mGuard);
    /*
     * Primero, tenemos que ver que no esté la partición montada del mapping
     * del cifrado. Si existe, desmontarla. Que esté montada implica que el
     * mapping del cifrado también está andando. Falla en tal caso.
     */
    if (isMountPoint(MOUNTPOINT))
        return false;

    /*
     * Cerramos el mapping de cifrado en cada vuelta y volvemos a abrir con 
     * las password pasada hasta poder montar un sistema de archivos FAT. 
     */
    std::cerr << "Searching partition..." << std::endl;
    bool success = false;
    for (unsigned short i = 0; i < SLOTS_AMOUNT && !success; i++)
    {
        if(mOperationCanceled)
        {
            std::cerr << "Mount operation canceled." << std::endl;
            mOperationCanceled = false;
            break;
        }
        closeMapping(ENCRYPTED);
        openCryptMapping(i, password);

        if((success = callMount()))
            std::cerr << "Partition found and mounted." << std::endl;
        else
            closeMapping(ENCRYPTED);
    }
    return success;
}

bool PartitionManager::unmountPartition()
{
    std::lock_guard<std::mutex> lock(mGuard);
    if (!isMountPoint(MOUNTPOINT))
        throw std::logic_error("No mounted partition");
    if (!callUmount())
        throw SysCallError("Unmount failed");

    if(rmdir(MOUNTPOINT.c_str()) != 0)
        return false;
    closeMapping(ENCRYPTED);
    return true;
}

void PartitionManager::resetProgress()
{
    mProgress = 0;
}

void PartitionManager::abortOperation()
{
    mOperationCanceled = true;
}

bool PartitionManager::isPartitionMounted()
{
    return isMountPoint(MOUNTPOINT);
}

void PartitionManager::ejectDevice()
{
    mCloseAtDestroy = true;
}

bool PartitionManager::isWraparoundOurs()
{
    return true;
}

bool PartitionManager::callMount()
{
    if (!opendir(MOUNTPOINT.c_str()))
        mkdir(MOUNTPOINT.c_str(), 0);
    chmod(MOUNTPOINT.c_str(),0777);
    return (mount((MAPPER_DIR+ENCRYPTED).c_str(),
                    MOUNTPOINT.c_str(),
                    "vfat", 0,
                    "dmask=000,fmask=000") == 0);
}

bool PartitionManager::callUmount()
{
    return (umount(MOUNTPOINT.c_str()) == 0);
}

PartitionManager::~PartitionManager()
{

    if(mCloseAtDestroy)
    {
        if(isPartitionMounted())
            unmountPartition();
        if(fileExists(MAPPER_DIR+ENCRYPTED))
            closeMapping(ENCRYPTED);
    }
    if(!isPartitionMounted() && fileExists(MAPPER_DIR+ENCRYPTED))
        closeMapping(WRAPAROUND);
}
