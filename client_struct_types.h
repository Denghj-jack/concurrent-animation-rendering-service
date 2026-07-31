#ifndef CLIENT_STRUCT_TYPES_H
#define CLIENT_STRUCT_TYPES_H

#include "configuration.h"
#include "common_io_use.h"


typedef struct client_info{
    int c2s_fd;
    int s2c_fd;
    int logged_in; //This represents thh user login status. 1->has login 0-> not yet login
    int waiting_login_response; //This represents whether the client is waiting for the response fro the server. 1 -> waiting 0-> not waiting
    char username[USERNAME_MAX_SIZE+1];
    read_line_buffer s2c_buffer;
}client_info;

#endif