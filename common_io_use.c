#include "common_io_use.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


int set_non_blocking(int fd){
    int flags = fcntl(fd , F_GETFL);
    if(flags == -1){
        return -1;
    }
    if(fcntl(fd , F_SETFL , flags | O_NONBLOCK) == -1){
        perror("Failed to set the Non-blocking mode");
        return -1;
    }
    return 0;
}


int read_message(int fd , read_line_buffer *buffer , char* single_message){
    while(1){
        char* newline_finder = memchr(buffer ->contents , '\n' , buffer ->length);
        if(newline_finder != NULL){//This indicates there exists a complete command now
            size_t command_length = (size_t)(newline_finder - buffer -> contents) + 1;

            memcpy(single_message , buffer -> contents , command_length);
            single_message[command_length] = '\0';

            size_t remaining_length = buffer ->length - command_length;
            memmove(buffer->contents , buffer ->contents + command_length , remaining_length);
            buffer -> length = remaining_length;
            return 0;//successful to read a single command
        }
        ssize_t words_num = read(fd , buffer ->contents + buffer->length , sizeof(buffer -> contents) - buffer -> length);
        if(words_num > 0){
            buffer -> length += (size_t)words_num;
            continue;
        }
        if(words_num == 0){
            return 2; //This implies that the opposite side is closed
        }
        if(errno == EINTR){
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK){//temporarily no message to read
            return 1; 
        }
        return 3;//This implies a real error exists
    }
}



int send_message(int fd , const char* message){
    if(message == NULL){
        return -1;
    }
    size_t message_length = strlen(message);
    size_t counter = 0;
    while(counter < message_length){
        ssize_t write_num = write(fd , message + counter , message_length - counter);
        if(write_num > 0){
            counter += (size_t)write_num;
            continue;
        }
        if(write_num == -1 && errno == EINTR){
            continue;
        }
        //This will prevent from busy waiting, as this tells us there's no message to write now
        //we can then temporaily return to the poll, and wait for new writing
        if(write_num == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){ 
            return 1;
        }
        return -1;
    }
    return 0;
}