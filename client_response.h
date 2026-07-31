#ifndef CLIENT_RESPONSE_H
#define CLIENT_RESPONSE_H

#include "client_struct_types.h"

//This function will print out the message to the stdout according to the reply from server
void print_rpc_response(char* reply_message);

//This function processes each server reply
int handler_one_server_reply(client_info* client_state , char* reply);


//This function processes the whole response reading from the fd at a time
int handle_server_reponse(client_info* client_state);


#endif