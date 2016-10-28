#include <iostream>
#include <string>
#include <cassert>
#include <thread>
#include <chrono>

#include "logic/PartitionManager.h"

void printProgress(const PartitionManager &pm)
{
    int progress = 0;
    do
    {
        progress = pm.getProgress();
        std::cout << progress << "%" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));   
    }while(progress != 100);
    
    
}

int main()
{   
    std::string dev;
    char yesno;
    
    std::ios_base::sync_with_stdio(false);
    
    std::cout << "Dispositivo: ";
    std::cin >> dev;
    {
        PartitionManager pm(dev);
        std::cout << "Wipe?";
        std::cin >> yesno;
        if(yesno == 'y')
        {
            std::thread progressReporter(printProgress,std::ref(pm));
            pm.WipeVolume();
            progressReporter.detach();
        }
            
    }
    return 0;
}