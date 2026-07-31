#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <sys/types.h>

//This function is used to register the signals handlers functions
//using sigaction(), and then send the SIGUSR1 singal to the server
//and close if the server did not send SIGUSR2 signal back within 1 second
int connection_to_server(pid_t server_pid , int* c2s_fd , int* s2c_fd);


#endif