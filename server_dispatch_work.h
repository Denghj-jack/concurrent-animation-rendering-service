#ifndef SERVER_DISPATCH_H
#define SERVER_DISPATCH_H


#include <stddef.h>
#include "server_struct_types.h"


//This function will split the RPC commands into several tokens
void run_rpc_command(client_entry* client , char* line , char* response , size_t response_size);


//This function will split the requests to different handlers, according to the first token of command
int process_rpc_command(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

#endif