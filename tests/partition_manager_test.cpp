#include <iostream>
#include <string>
#include <cassert>
#include <thread>
#include <chrono>

#include "logic/PartitionManager.h"

static void PrintProgress(const PartitionManager &pm)
{
    int progress = 0;
    do
    {
        progress = pm.getProgress();
        std::cout << progress << "%" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));   
    }while(progress != 100);
}


static void Wipe(PartitionManager &pm)
{
    std::thread progressReporter(PrintProgress,std::ref(pm));
    pm.WipeVolume();
    progressReporter.detach();
}

static void Create(PartitionManager &pm)
{
    std::string password;
    unsigned short offset;
    std::cout << "Password: ";
    std::cin >> password;
    std::cout << "Offset: "; 
    std::cin >> offset;
    pm.CreatePartition(offset,password);
}

static void Mount(PartitionManager &pm)
{
    std::string password;
    std::cout << "Password: ";
    std::cin >> password;
    pm.MountPartition(password);
}

static void Unmount(PartitionManager &pm)
{
    pm.UnmountPartition();
}


int main()
{   
    std::string dev;
    std::ios_base::sync_with_stdio(false);
    std::cout << "Dispositivo: ";
    std::cin >> dev;
    {
        PartitionManager pm(dev);
        int choice;

        do
        {
            std::cout << "1. Wipe" << std::endl;
            std::cout << "2. Crear partición" << std::endl;
            std::cout << "3. Montar partición" << std::endl;
            std::cout << "4. Desmontar partición" << std::endl;
            std::cout << "5. Extraer disco" << std::endl;
    
            std::cout << "0. Salir" << std::endl;
            std::cout << "Opción: ";
            std::cin >> choice;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            switch(choice)
            {
            case 0:
                std::cout << "Saliendo..." << std::endl; 
                break;    
            case 1:
                Wipe(pm);
                break;
            case 2:
                Create(pm);
                break;
            case 3:
                Mount(pm);
                break;
            case 4:
                Unmount(pm);
                break;
            case 5:
                break;
            default:
                std::cout << "Opción inválida." << std::endl;
                break;
            }
            std::cout << "OK" << std::endl;
        }while(choice != 0);
    }
    return 0;
}