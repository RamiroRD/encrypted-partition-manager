#include <sys/mount.h>
#include <sys/stat.h>
#include <syscall.h>
#include <linux/random.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <mntent.h>
#include <dirent.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>


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

PartitionManager::PartitionManager(const std::string &device)
    : mCurrentDevice(device),
      mDeviceSize(currentDeviceSize()/BLOCK_SIZE_),
      mOffsetMultiple(mDeviceSize / SLOTS_AMOUNT),
      mProgress(0),
      mOperationCanceled(false),
      mCloseAtDestroy(false),
      mCrypto()
{
     unmountAll();


     std::cerr << device << " has " << mDeviceSize << " blocks."
             << std::endl;
     std::cerr << "major " << major(mFileStat.st_rdev) << std::endl;
     std::cerr << "minor " << minor(mFileStat.st_rdev) << std::endl;


    /*
     * Verificar wraparound y /mnt/part.
     */
    if(fileExists(MAPPER_DIR+WRAPAROUND))
    {
        std::cerr << "Wraparound already open." << std::endl;
        if(!isWraparoundOurs())
        {
            std::cerr << "Wraparound does not belong to this device." << std::endl;
            if(isPartitionMounted())
                unmountPartition();
            if(fileExists(MAPPER_DIR+ENCRYPTED))
                closeMapping(ENCRYPTED);
            closeMapping(MAPPER_DIR+WRAPAROUND);
            createWraparound();
        }else{
            if(isPartitionMounted())
                std::cerr << "Partition already mounted." << std::endl;
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

    if (pclose(stream) != 0)
        throw CommandError("dmsetup create wraparound");
    std::cerr << "Wraparound opened." << std::endl;
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
    const uint64_t offset = slot * mOffsetMultiple;
    std::stringstream ss;
    ss << "cryptsetup --hash=sha512 --cipher=aes-xts-plain64 --key-size=512";
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
    if (fileExists(MAPPER_DIR+logicalDevice))
    {
        std::string cmd = "dmsetup remove --force " + logicalDevice;
        if(system(cmd.c_str()) != 0)
            throw CommandError(cmd);
    }
}

unsigned char PartitionManager::getProgress() const
{
    return mProgress;
}

bool PartitionManager::wipeDevice()
{
    if(isMountPoint(MOUNTPOINT))
        throw std::logic_error("Partition mounted!");
    if(mProgress != 0)
        throw std::logic_error("resetProgress() not previously called!");

    std::vector<char> key(256);
    key[256] = '\0';
    int random = open("/dev/urandom", O_RDONLY);
    if(random == -1)
        throw SysCallError("open",errno);
    read(random,key.data(),255);
    close(random);

    openCryptMapping(0,std::string(key.begin(),key.end()));
    int out = open((MAPPER_DIR+ENCRYPTED).c_str(), O_WRONLY);
    if(out == -1)
        throw SysCallError("open",errno);

    std::cerr << "Wiping device ..." << std::endl;

    try
    {
        const uint64_t deviceSize = mDeviceSize * BLOCK_SIZE_;
        std::vector<char> buffer(MAX_WRITE_SIZE,'\0');
        mProgress = 0;
        unsigned amount = std::min(deviceSize, MAX_WRITE_SIZE);
        for (uint64_t i = 0;
             i < deviceSize && !mOperationCanceled;
             i += (amount = std::min(deviceSize - i, MAX_WRITE_SIZE)))
        {
            if(write(out,buffer.data(),amount) == -1)
                throw SysCallError("write",errno);
            fsync(out);
            mProgress = std::floor(100 * i / deviceSize);
        }
        if(mOperationCanceled)
        {
            std::cout << "Wipe canceled." << std::endl;
            mOperationCanceled = false;
        }
        mProgress = 100;
        std::cerr << "Device wiped." << std::endl;
        close(out);
        closeMapping(ENCRYPTED);
    }catch(SysCallError &e){
        close(out);
        closeMapping(ENCRYPTED);
        throw e;
    };


    return true;

}

bool PartitionManager::createPartition(const unsigned short slot,
                                       const std::string &password)
{
    if (isMountPoint(MOUNTPOINT))
        throw std::logic_error("Partition is already mounted");
    if (slot >= SLOTS_AMOUNT)
        throw std::domain_error("Slot number out of range");
    if (password.empty())
        throw std::domain_error("Empty password");
    
    //closeMapping(ENCRYPTED);
    openCryptMapping(slot,password);

    std::string cmd = "mkfs.vfat " + MAPPER_DIR + ENCRYPTED;
    if(system(cmd.c_str())!=0)
        throw CommandError(cmd);
    std::cerr << "FAT filesystem created." << std::endl;
    if(!callMount())
        throw SysCallError("mount",errno);
    std::cerr << "Filesystem mounted." << std::endl;

    return true;
}

bool PartitionManager::mountPartition(const std::string &password)
{
    /*
     * Primero, tenemos que ver que no esté la partición montada del mapping
     * del cifrado. Si existe, desmontarla. Que esté montada implica que el
     * mapping del cifrado también está andando. Falla en tal caso.
     */
    if (isMountPoint(MOUNTPOINT))
        return false;

    if(password.empty())
        throw std::domain_error("Empty password.");

    int fd = open((MAPPER_DIR+WRAPAROUND).c_str(), O_RDONLY);
    if(fd == -1)
        throw SysCallError("open",errno);


    std::cerr << "Searching partition..." << std::endl;
    std::vector<char> header(512);
    bool found = false;
    unsigned short successfulSlot;
	lseek64(fd,0,SEEK_CUR);
    for (unsigned short i = 0; i < SLOTS_AMOUNT; i++)
    {
        if(mOperationCanceled)  
            break;

        lseek64(fd,mOffsetMultiple*BLOCK_SIZE_,SEEK_CUR);
        read(fd,header.data(),512);

        std::vector<char> decryptedHeader = mCrypto.decryptMessage(header,password);
        if(decryptedHeader[510] == (char) 0x55 &&
           decryptedHeader[511] == (char) 0xAA)
        {
            std::cerr << "FAT signature found!" << std::endl;
            successfulSlot = i;
            found = true;
            break;
        }
    }
    close(fd);
    if(mOperationCanceled)
    {
        std::cerr << "Mount operation canceled." << std::endl;
        mOperationCanceled = false;
    }
    if(found)
    {
        openCryptMapping(successfulSlot, password);
        if(callMount())
        {
            std::cerr << "Partition mounted." << std::endl;
            return true;
        }
        else
            throw SysCallError("mount");
    }else
    {
        std::cerr << "FAT signature not found." << std::endl;
        throw PartitionNotFoundException();
    }
    return found;
}

bool PartitionManager::unmountPartition()
{
    if (!isMountPoint(MOUNTPOINT))
        throw std::logic_error("No mounted partition");
    if (!callUmount())
        throw SysCallError("Unmount failed");

    if(rmdir(MOUNTPOINT.c_str()) != 0)
        return false;
    closeMapping(ENCRYPTED);
    std::cerr << "Partition unmounted." << std::endl;
    return true;
}

void PartitionManager::resetProgress()
{
    mProgress = 0;
}

void PartitionManager::abortOperation()
{
    std::cerr << "Abort requested." << std::endl;
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
    auto pair = currentDeviceID();
    return (pair.first == major(mFileStat.st_rdev) &&
           pair.second == minor(mFileStat.st_rdev));
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

const std::list<std::string> PartitionManager::findAllDevices()
{
    DIR * dirStream;
    struct dirent * entry;
    std::list<std::string> list;

    if(!(dirStream = opendir("/dev")))
        throw SysCallError("opendir(\"/dev\"", errno);
    for(entry = readdir(dirStream); entry; entry = readdir(dirStream))
    {
        if(strlen(entry->d_name)==3 &&
           entry->d_name[0] == 's' &&
           entry->d_name[1]=='d')
            list.push_back(std::string("/dev/") +entry->d_name);
    }
    closedir(dirStream);
    list.sort();
    return list;
}

// wraparound tiene que estar abierto
const std::pair<unsigned,unsigned> PartitionManager::currentDeviceID()
{
    const char * cmd = "dmsetup table wraparound";
    FILE * output = popen(cmd,"r");
    if(!output)
        throw SysCallError("popen",errno);
    std::vector<char> buffer(255);
    fgets(buffer.data(),255,output);
    pclose(output);

    unsigned major, minor;
    sscanf(buffer.data(),"%*d %*d %*s %d:%d %*d\n",&major,&minor);
    return std::pair<int,int>(major,minor);
}

const std::string PartitionManager::currentDevice()
{
    if(!fileExists(MAPPER_DIR+WRAPAROUND))
        return "";
    auto devID = currentDeviceID();
    struct stat walk;
    for(auto &devicePath : findAllDevices())
    {
        if(stat(devicePath.c_str(),&walk) == -1)
        {
            std::cerr << "Could not stat " << devicePath << ":";
            perror("");
            continue;
        }
        if(S_ISBLK(walk.st_mode) &&
           major(walk.st_rdev) == devID.first &&
           minor(walk.st_rdev) == devID.second)
        {
            // encontramos el path del dispositivo
            std::cerr << "Found device: " << devicePath << std::endl;
            return devicePath;
        }
    }
    return "";
}

void PartitionManager::unmountAll()
{
    std::string line;
    std::cerr << "Trying to unmount all mountpoints of " << mCurrentDevice << std::endl;
    std::ifstream mountFile("/proc/mounts");
    if(!mountFile.is_open())
        return;
    for(std::getline(mountFile,line); !line.empty(); std::getline(mountFile,line))
    {
        std::stringstream ss(line);
        std::string device;
        ss >> device;

        if(device.find(mCurrentDevice) != std::string::npos)
        {
            std::string mountpoint;
            ss >> mountpoint;
            if(umount(mountpoint.c_str()) == 0)
                std::cerr << mountpoint << " unmounted." << std::endl;
            else
			{
                std::cerr << "Failed to unmount " << mountpoint << "." << std::endl;
				perror("");
			}
        }
    }
}

uint64_t PartitionManager::currentDeviceSize()
{
    int fd = open(mCurrentDevice.c_str(),O_RDWR);

    if(fd == -1)
    {
       perror("");
       if(errno == ENOENT)
           throw std::domain_error("No such device.");
       else if (errno == EPERM || errno == EACCES)
           throw std::runtime_error("Permission denied.");
       else
           throw std::runtime_error("Unknown error when opening device block file.");
    }else{
       unsigned long long size;
       if(fstat(fd,&mFileStat) == -1)
           throw SysCallError("stat", errno);

       if(!S_ISBLK(mFileStat.st_mode))
           throw std::domain_error("Not a block file!");
       if(ioctl(fd, BLKGETSIZE64, &size)==-1)
           throw SysCallError("ioctl BLKGETSIZE64",errno);
       close(fd);
       return size;
    }
}

PartitionManager::~PartitionManager()
{
    try
    {
        if(mCloseAtDestroy)
        {
            if(isPartitionMounted())
                unmountPartition();
        }
        if(!isPartitionMounted())
        {
            if(fileExists(MAPPER_DIR+ENCRYPTED))
                closeMapping(ENCRYPTED);
            if(fileExists(MAPPER_DIR+WRAPAROUND))
                closeMapping(WRAPAROUND);
            std::cerr << "No partition mounted. Wraparound closed." << std::endl;
        }
     }catch(std::exception &e){
        std::cerr << "Exception caught while destroying PartitionManager: "
                  << e.what();
    }catch(...){
        std::cerr << "Unknown exception caught while destroying PartitionManager."
                  << std::endl;
    }
}
