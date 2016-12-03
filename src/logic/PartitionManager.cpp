#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cmath>


#include "logic/PartitionManager.h"

static bool fileExists(const std::string &path)
{
    std::ifstream f(path);
    return f.good();
}

bool PartitionManager::logicalDeviceExists(const std::string &logicalDev)
{
    std::string filePath = PartitionManager::MAPPINGS_FOLDER_PATH;
    filePath += logicalDev;
    return fileExists(filePath);
}

/*
  * Verifica que exista el dispositivo y crea el mapping wraparound si ya no
  * existiera.
  */
PartitionManager::PartitionManager(const std::string &device)
    : mDeviceSize(0),
      mOffsetMultiple(0),
      mCurrentDevice(device),
      mProgress(0),
      mOperationCanceled(false)
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
        struct stat fileStat;
        unsigned long long size;
        fstat(fd,&fileStat);
        if(!S_ISBLK(fileStat.st_mode))
            throw std::domain_error("Not a block file!");
        if(ioctl(fd, BLKGETSIZE64, &size)==-1)
            throw SysCallError("ioctl BLKGETSIZE64",errno);
        
        mDeviceSize = size/BLOCK_SIZE_;
        mOffsetMultiple = mDeviceSize / SLOTS_AMOUNT;
        std::cerr << device << " has " << mDeviceSize << " blocks."
                << std::endl;
     }

    /*
     * Ver que exista el wraparound y crearlo si no existe.
     */
    std::string path = MAPPINGS_FOLDER_PATH;
    path += WRAPAROUND_DEVICE_NAME;
    if (!fileExists(path))
    {
        std::cerr << "Wraparound device not open." << std::endl;
        createWraparound();
    }
}

void PartitionManager::createWraparound()
{
    std::stringstream table;
    table << 0 << " " << mDeviceSize << " linear " << mCurrentDevice << " "
          << 0 << std::endl
          << mDeviceSize << " " << mDeviceSize << " linear " << mCurrentDevice << " "
          << 0 << std::endl;

    FILE *stream = popen("dmsetup create wraparound", "w");

    std::string input = table.str();
    fwrite(input.c_str(), 1, input.size(), stream);

    std::cerr << "Opening wraparound ..." << std::endl;
    if (pclose(stream) != 0)
        throw CommandError("dmsetup create wraparound");
}

// dirPath sin la barra!
// Se fija si el directorio y sus contenidos están en el mismo dispositivo
bool PartitionManager::isMountPoint(const std::string &dirPath)
{
    struct stat dirStat;
    struct stat parentStat;
    const std::string parent = dirPath + "/..";


    if(stat(dirPath.c_str(),&dirStat) !=0)
        return false;
    if(!S_ISDIR(dirStat.st_mode))
        return false;
    if(stat(parent.c_str(),&parentStat) !=0)
        return false;
    
    return dirStat.st_dev != parentStat.st_dev;
}

void PartitionManager::openCryptMapping(const unsigned short slot,
                                        const std::string &password)
{
    const off_t offset = slot * mOffsetMultiple;
    char *cmd;
    asprintf(&cmd, "cryptsetup --hash=sha512 --cipher=aes-xts-plain64 --offset %jd open --size %jd --type=plain %s%s %s",
             offset, mDeviceSize, MAPPINGS_FOLDER_PATH,
             WRAPAROUND_DEVICE_NAME, ENCRYPTED_DEVICE_NAME);

    FILE *stream = popen(cmd, "w");
    fwrite(password.c_str(), 1, password.size(), stream);
    fputc('\n', stream);
    free(cmd);
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

    if(isMountPoint(MOUNT_POINT))
        return false;
    if(mProgress != 0)
        return false;

    std::string path = MAPPINGS_FOLDER_PATH;
    path += WRAPAROUND_DEVICE_NAME;

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
    if (isMountPoint(MOUNT_POINT))
        throw std::logic_error("Partition is already mounted");
    if (slot >= SLOTS_AMOUNT)
        throw std::domain_error("Slot number out of range");
    if (password.empty())
        throw std::domain_error("Empty password");
    
    closeMapping(ENCRYPTED_DEVICE_NAME);
    openCryptMapping(slot,password);

    std::string cmd = "mkfs.vfat ";
    cmd += MAPPINGS_FOLDER_PATH;
    cmd += ENCRYPTED_DEVICE_NAME;
    if(system(cmd.c_str())!=0)
        return false;
    if (!opendir(MOUNT_POINT))
        mkdir(MOUNT_POINT, 0);
    chmod(MOUNT_POINT,0777);
    std::string encryptedPath = MAPPINGS_FOLDER_PATH;
    encryptedPath += ENCRYPTED_DEVICE_NAME;
    if(mount(encryptedPath.c_str(), MOUNT_POINT, "vfat", 0, "fmask=000,dmask=000")==0)
        return true;
    else
        return false;

}

bool PartitionManager::mountPartition(const std::string &password)
{
    std::lock_guard<std::mutex> lock(mGuard);
    /*
     * Primero, tenemos que ver que no esté la partición montada del mapping
     * del cifrado. Si existe, desmontarla. Que esté montada implica que el
     * mapping del cifrado también está andando. Falla en tal caso.
     */
    if (isMountPoint(MOUNT_POINT))
        return false;

    /*
     * Cerramos el mapping de cifrado en cada vuelta y volvemos a abrir con 
     * las password pasada hasta poder montar un sistema de archivos FAT. 
     */
    std::cerr << "Searching partition..." << std::endl;
    if (!opendir(MOUNT_POINT))
        mkdir(MOUNT_POINT, 0);
    chmod(MOUNT_POINT,0777);
    bool success = false;
    for (unsigned short i = 0; i < SLOTS_AMOUNT && !success; i++)
    {
        if(mOperationCanceled)
        {
            std::cerr << "Mount operation canceled." << std::endl;
            mOperationCanceled = false;
            break;
        }
        closeMapping(ENCRYPTED_DEVICE_NAME);
        openCryptMapping(i, password);
        std::string encryptedPath = MAPPINGS_FOLDER_PATH;
        encryptedPath += ENCRYPTED_DEVICE_NAME;
        success = mount(encryptedPath.c_str(), MOUNT_POINT, "vfat", 0, "dmask=000,fmask=000") == 0;

        if(!success)
            closeMapping(ENCRYPTED_DEVICE_NAME);
        else
            std::cerr << "Partition found and mounted." << std::endl;
    }
    return success;
}

bool PartitionManager::unmountPartition()
{
    std::lock_guard<std::mutex> lock(mGuard);
    int exitCode;
    if (!isMountPoint(MOUNT_POINT))
        throw std::logic_error("No mounted partition");

    if ((exitCode = umount(MOUNT_POINT)) != 0)
        throw SysCallError("Unmount failed",exitCode);

    if(rmdir(MOUNT_POINT) != 0)
        return false;
    closeMapping(ENCRYPTED_DEVICE_NAME);
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
    return isMountPoint(MOUNT_POINT);
}

PartitionManager::~PartitionManager()
{
}
