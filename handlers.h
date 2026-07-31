#ifndef HANDLERS_H
#define HANDLERS_H

#include <stddef.h>
#include "server_struct_types.h"




int handle_create_sprite(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_create_rectangle(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_create_circle(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_destroy_sprite(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_place_sprite(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_placement_operation(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_destroy_placement(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_create_canvas(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_destroy_canvas(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_set_animation_params(client_entry* client , rpc_commands* commands , char* response , size_t response_size);

int handle_generate(client_entry* client, rpc_commands* commands, char* response, size_t response_size);
int handle_shared_canvas(client_entry* client, rpc_commands* commands, char* response, size_t response_size);

//Handle the barrier RPC
int handle_barrier(client_entry* client, rpc_commands* commands, char* response, size_t response_size);

#endif
