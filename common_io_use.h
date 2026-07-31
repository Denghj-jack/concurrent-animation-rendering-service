#ifndef COMMON_IO_USE_H
#define COMMON_IO_USE_H

#include <stddef.h>
#include "configuration.h"



typedef struct read_buffer{
    char contents[INPUT_MAX_SIZE];
    size_t length;
} read_line_buffer;


int set_non_blocking(int fd);

//This will read a complete command, with the help of new line character
//this prevents from reading uncomplete commands
int read_message(int fd , read_line_buffer *buffer , char* single_message);


//This will write a complete command, with the help of new line character
//this prevents from write uncomplete commands
int send_message(int fd , const char* message);


#endif