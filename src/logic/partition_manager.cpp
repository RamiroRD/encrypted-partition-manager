#include <cassert>

#include "logic/partition_manager.h"

static const unsigned PARTITIONS_AMOUNT = 8192;

static int current_device = -1;

static off_t offset_multiple = 0;

static error_code last_error = error_code::no_error;


int set_working_volume(const std::string &device)
{
    
    // Check if file exists
    // Check size 
    return 0;
}

int init_volume()
{
    if(current_device == -1)
    {
        last_error = error_code::device_not_open;
        return -1;
    }
    assert(offset_multiple != 0);
    return 0;
}

int create_partition(const off_t offset, const std::string &password)
{
    return 0;
}

int mount_partition(const std::string &device, const std::string &password)
{
    return 0;
}

error_code get_last_error()
{
    return last_error;
}