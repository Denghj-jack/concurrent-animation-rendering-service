#ifndef RPC_PREPROCESS_H
#define RPC_PREPROCESS_H


#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "server_struct_types.h"

//These five functions are used to create the corrsponding message sending from the server to client, in specific format
//and store it in the paramater response
int create_success_message(char* response, size_t response_size);
int create_success_uint64_message(char* response, size_t response_size, uint64_t return_value);
int create_internal_error_message(char* response, size_t response_size);
int create_value_error_message(char* response, size_t response_size);
int create_rpc_error_message(char* response, size_t response_size);


int split_rpc_commands(char* line, rpc_commands* commands);
int num_tokens_check(int token_count, int required_num);


//These five functions are used to check whether the inputs from client are in valid format.
int convert_uint64_parameter(char* token, uint64_t* id);

//This function is used to check the correctness of width/height/radius and convert into size_t type
int convert_size_t_token(char* token, size_t* value);

//This function is used to check whether the color variable is correct
int convert_color_t_token(char* token, color_t* value);
int convert_bool_token(char* token, int* value);
int convert_ssize_t_token(char* token, ssize_t* value);


#endif