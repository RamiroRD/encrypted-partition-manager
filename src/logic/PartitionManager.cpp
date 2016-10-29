#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <cstdio>

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
      mProgress(0)
{
    /*
     * Verificamos que el dispositivo exista. Si no existe, entonces se levanta
     * una excepción que debería manejarse desde el caller del constructor.
     */
    try
    {
        std::fstream dev;
        dev.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        dev.open(device.c_str(), std::ios_base::binary | std::ios_base::in);
        dev.seekg(0, std::ios_base::end);
        assert(dev.tellg() % BLOCK_SIZE_ == 0);
        mDeviceSize = dev.tellg() / BLOCK_SIZE_;
        mOffsetMultiple = mDeviceSize / SLOTS_AMOUNT;
        dev.close();
    }
    catch (std::ifstream::failure e)
    {
        std::cerr << e.what();
        throw std::domain_error("No existe tal dispositivo.");
    }

    std::cerr << "El dispositivo tiene " << mDeviceSize << " bloques."
              << std::endl;

    /*
     * Ver que exista el wraparound y crearlo si no existe.
     */
    std::string path = MAPPINGS_FOLDER_PATH;
    path += WRAPAROUND_DEVICE_NAME;
    if (!fileExists(path))
    {
        std::cerr << "Wraparound no existe." << std::endl;
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

    std::cerr << "Creando wraparound ..." << std::endl;
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
    return mProgress.load();
}

void PartitionManager::WipeVolume()
{
    if(isMountPoint(MOUNT_POINT))
        throw std::logic_error("Hay una partición montada.");

    std::ifstream random;
    random.open("/dev/urandom", std::ios_base::binary | std::ios_base::in);
    random.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    std::string path = MAPPINGS_FOLDER_PATH;
    path += WRAPAROUND_DEVICE_NAME;
    std::ofstream out;
    out.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    out.open(path, std::ios_base::binary | std::ios_base::out);

    std::cerr << "Wipeando volumen..." << std::endl;

    char buffer[BLOCK_SIZE_ * MAX_TRANSFER_SIZE];
    for (off_t i = 0; i < mDeviceSize;)
    {
        unsigned blocks = std::min(mDeviceSize - i, MAX_TRANSFER_SIZE);
        random.read(buffer, blocks * BLOCK_SIZE_);
        out.write(buffer, blocks * BLOCK_SIZE_);
        /*
         * TODO: habiendo sido ejecutado una vez, el progreso queda en 100.
         * Debería haber algún mecanismo para ponerlo en cero ANTES de volver a
         * entrar a este método (Un thread simple que termina de pollear
         * mProgress cuando el progreso es 100 puede terminar prematuramente).
         */
        mProgress = 100 * i / mDeviceSize;
        i += blocks;
    }
    mProgress = 100;
    // TODO: posiblemente atrapar excepciones?
    std::cerr << "Volumen wipeado!" << std::endl;
}

void PartitionManager::CreatePartition(const unsigned short slot,
                                       const std::string &password)
{
    if (isMountPoint(MOUNT_POINT))
        throw std::logic_error("Hay una partición montada.");
    if (slot > SLOTS_AMOUNT)
        throw std::domain_error("Número de slot fuera de rango.");
    if (password.empty())
        throw std::domain_error("Contraseña vacía.");
    
    closeMapping(ENCRYPTED_DEVICE_NAME);
    openCryptMapping(slot,password);

    std::string cmd = "mkfs.vfat ";
    cmd += MAPPINGS_FOLDER_PATH;
    cmd += ENCRYPTED_DEVICE_NAME;
    if(system(cmd.c_str())!=0)
        throw CommandError(cmd);
    if (!opendir(MOUNT_POINT))
        mkdir(MOUNT_POINT, 0777);
    std::string encryptedPath = MAPPINGS_FOLDER_PATH;
    encryptedPath += ENCRYPTED_DEVICE_NAME;
    mount(encryptedPath.c_str(), MOUNT_POINT, "vfat", 0, "");
}

void PartitionManager::MountPartition(const std::string &password)
{
    /*
     *
     * Primero, tenemos que ver que no esté la partición montada del mapping
     * del cifrado. Si existe, desmontarla. Que esté montada implica que el
     * mapping del cifrado también está andando. Falla en tal caso.
     */
    if (isMountPoint(MOUNT_POINT))
        throw std::logic_error("Partición todavía montada.");

    /*
     * Cerramos el mapping de cifrado en cada vuelta y volvemos a abrir con 
     * las password pasada hasta poder montar un sistema de archivos FAT. 
     */
    std::cerr << "Buscando partición..." << std::endl;
    if (!opendir(MOUNT_POINT))
        mkdir(MOUNT_POINT, 0777);
    bool success = false;
    for (unsigned short i = 0; i < SLOTS_AMOUNT; i++)
    {
        closeMapping(ENCRYPTED_DEVICE_NAME);
        openCryptMapping(i, password);
        std::string encryptedPath = MAPPINGS_FOLDER_PATH;
        encryptedPath += ENCRYPTED_DEVICE_NAME;
        int mountResult = mount(encryptedPath.c_str(), MOUNT_POINT, "vfat", 0, "");

        if (mountResult == 0)
        {
            success = true;
            break;
        }

        closeMapping(ENCRYPTED_DEVICE_NAME);
    }
    if (!success)
        throw PartitionNotFoundException();
}

void PartitionManager::UnmountPartition()
{
    int exitCode;
    if (!isMountPoint(MOUNT_POINT))
        throw std::logic_error("No hay partición montada.");

    if ((exitCode = umount(MOUNT_POINT)) != 0)
        throw SysCallError("umount falló.",exitCode);

    rmdir(MOUNT_POINT);
    closeMapping(ENCRYPTED_DEVICE_NAME);
}

PartitionManager::~PartitionManager()
{
}
