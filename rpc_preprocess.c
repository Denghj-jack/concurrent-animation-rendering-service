#include "rpc_preprocess.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int create_success_message(char* response , size_t response_size){
    snprintf(response , response_size , "0\n");
    return 0;
}

int create_success_uint64_message(char* response , size_t response_size , uint64_t return_value){
    snprintf(response , response_size , "0 %" PRIu64 "\n" , return_value);
    return 0;
}

int create_internal_error_message(char* response , size_t response_size){
    snprintf(response , response_size , "-3\n");
    return 0;
}

int create_value_error_message(char* response , size_t response_size){
    snprintf(response , response_size , "-2\n");
    return 0;
}

int create_rpc_error_message(char* response , size_t response_size){
    snprintf(response , response_size , "-1\n");
    return 0;
}



int split_rpc_commands(char* line , rpc_commands* commands){
    if(line == NULL || commands == NULL){
        return -1;
    }

    strncpy(commands ->buffer , line , sizeof(commands -> buffer)-1);//Just in case the commands is very long
    commands-> buffer[sizeof(commands->buffer) -1] = '\0';
    
    char* last_point = NULL;
    char* token = NULL;
    token = strtok_r(commands -> buffer , " \n" , &last_point);
    while(token != NULL){
        if(commands -> token_count >= MAX_RPC_PARAMETERS){
            return -1;
        }
        commands -> tokens[commands -> token_count] = token;
        commands -> token_count++;
        token = strtok_r(NULL , " \n" , &last_point);
    }
    return 0;
}


int num_tokens_check(int token_count, int required_num){
    return token_count==required_num;
}


int convert_uint64_parameter(char* token , uint64_t* id){
    if(token == NULL || id == NULL){
        return -1;
    }
    if(token[0] == '-'){ //cannot be negative number
        return -1;
    }
    char* end = NULL;
    unsigned long long converted_num;
    errno = 0;
    converted_num = strtoull(token , &end , 10);
    if(errno != 0 || end == token || *end != '\0'){
        return -1;
    }
    *id = (uint64_t)converted_num;
    return 0;
}

int convert_size_t_token(char* token , size_t* value){
    if(token == NULL || value == NULL){
        return -1;
    }
    if(token[0] == '-'){ //cannot be negative number
        return -1;
    }
    char* end = NULL;
    unsigned long long converted_num;
    errno = 0;
    converted_num = strtoull(token , &end , 10);
    if(errno != 0 || end == token || *end != '\0'){
        return -1;
    }
    *value = (size_t)converted_num;
    return 0;
}



int convert_color_t_token(char* token , color_t* value){
    if(token == NULL || value == NULL){
        return -1;
    }
    if(token[0] == '-'){ //cannot be negative number
        return -1;
    }
    char* end = NULL;
    unsigned long long converted_num;
    errno = 0;
    converted_num = strtoull(token , &end , 10);
    if(errno != 0 || end == token || *end != '\0'){
        return -1;
    }
    *value = (color_t)converted_num;
    return 0;
}


int convert_bool_token(char* token , int* value){
    if(token == NULL || value == NULL){
        return -1;
    }
    if(token[0] == '-'){ //cannot be negative number
        return -1;
    }
    char* end = NULL;
    unsigned long long converted_num;
    errno = 0;
    converted_num = strtoull(token , &end , 10);
    if(errno != 0 || end == token || *end != '\0'){
        return -1;
    }
    if(converted_num != 0 &&  converted_num != 1){
        return -1;
    }
    *value = (int)converted_num;
    return 0;
}

int convert_ssize_t_token(char* token , ssize_t* value){
    if(token == NULL || value == NULL){
        return -1;
    }
    char* end = NULL;
    long long converted_num;
    errno = 0;
    converted_num = strtoll(token , &end , 10);
    if(errno != 0 || end == token || *end != '\0'){
        return -1;
    }
    *value = (ssize_t)converted_num;
    return 0;
}