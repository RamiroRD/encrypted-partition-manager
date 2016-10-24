#ifndef PARTITION_MANAGER_H
#define PARTITION_MANAGER_H

#include <fcntl.h>
#include <unistd.h>
#include <string>

enum error_code
{no_error,
file_doesnt_exist,
device_not_open,
wrong_password};

int set_working_volume(const std::string &device);

int init_volume();

int create_partition(const off_t offset,
                     const std::string &password);

int mount_partition(const std::string &device, const std::string &password);

error_code get_last_error();



#endif