#include <cassert>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "logic/PartitionManager.h"


static bool fileExists(const std::string &path)
{
    std::ifstream f(path);
    return f.good();
}

 /*
  * Verifica que exista el dispositivo y crea el mapping wraparound si ya no
  * existiera.
  */
PartitionManager::PartitionManager(const std::string &device)
: 
  mDeviceSize(0),
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
        std::cerr << "Intentando abrir  " << device << " ..." << std::endl;
        std::fstream dev;
        dev.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        dev.open(device.c_str(), std::ios_base::binary | std::ios_base::in);
        dev.seekg(0,std::ios_base::end);
        assert(dev.tellg() % BLOCK_SIZE == 0);
        mDeviceSize = dev.tellg()/BLOCK_SIZE;
        mOffsetMultiple=mDeviceSize/PARTITIONS_AMOUNT;
        dev.close();  
    }catch(std::ifstream::failure e)
    {
        std::cerr << e.what();
        throw NoSuchDeviceException();
    }

    std::cerr << "El dispositivo tiene " << mDeviceSize << " bloques." 
    << std::endl;
    
    /*
     * Ver que exista el wraparound y crearlo si no existe.
     */
     std::string path = MAPPINGS_FOLDER_PATH;
     path += WRAPAROUND_DEVICE_NAME;
     if(!fileExists(path))
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
    
    FILE * stream = popen("dmsetup create wraparound","w");
    
    std::string input = table.str();
    fwrite(input.c_str(),1,input.size(),stream);
    
    
    std::cerr << "Creando wraparound ..." << std::endl;
    if(pclose(stream)!=0)
        throw ExternalErrorException();
}

void PartitionManager::WipeVolume()
{   
    std::ifstream random;
    random.open("/dev/urandom",std::ios_base::binary | std::ios_base::in);
    random.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    std::string path = MAPPINGS_FOLDER_PATH; 
    path += WRAPAROUND_DEVICE_NAME;
    std::ofstream out;
    out.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    out.open(path,std::ios_base::binary | std::ios_base::out);
    
    
    char buffer [BLOCK_SIZE*MAX_TRANSFER_SIZE];
    for(off_t i = 0; i<mDeviceSize;)
    {
        unsigned blocks = std::min(mDeviceSize-i, MAX_TRANSFER_SIZE); 
        random.read(buffer,blocks*BLOCK_SIZE);
        out.write(buffer,blocks*BLOCK_SIZE);
        mProgress = 100* i/mDeviceSize;
        i += blocks;
    }
    std::cerr << "Volumen inicializado!" << std::endl;
}

void PartitionManager::CreatePartition(const unsigned short offset,
                                       const std::string &password)
{
    if(offset > PARTITIONS_AMOUNT)
        throw std::domain_error("Offset number out of range.");
    
}

void PartitionManager::MountPartition(const std::string &password)
{
}

void PartitionManager::UnmountPartition()
{
    
}

void PartitionManager::closeMapping()
{
    std::string filePath = PartitionManager::MAPPINGS_FOLDER_PATH;
    filePath += PartitionManager::ENCRYPTED_DEVICE_NAME;                  
    
    if(fileExists(filePath))
    {
        std::string cmd = "dmsetup remove --force";
        cmd += PartitionManager::ENCRYPTED_DEVICE_NAME;
        system(cmd.c_str());
    }
        
}

unsigned char PartitionManager::getProgress() const
{
    return mProgress.load();
}


PartitionManager::~PartitionManager()
{
 
}
