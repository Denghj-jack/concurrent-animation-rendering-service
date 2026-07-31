#ifndef SERVER_STRUCT_TYPES_H
#define SERVER_STRUCT_TYPES_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <pthread.h>

#include "configuration.h"
#include "common_io_use.h"

typedef uint32_t color_t;
typedef struct request_entry request_entry;
typedef struct response_entry response_entry;


//This represents a splitting rpc commands
typedef struct rpc_commands{
    char buffer[INPUT_MAX_SIZE];
    char* tokens[MAX_RPC_PARAMETERS];
    int token_count;
} rpc_commands;


typedef struct client_entry{
    pid_t pid;
    char fifo_c2s_path[PIPE_PATH_MAX_SIZE];
    char fifo_s2c_path[PIPE_PATH_MAX_SIZE];
    int c2s_fd;
    int s2c_fd;
    struct client_entry* next; //The linked list does not need to consider the reallocate the client array
    char username[USERNAME_MAX_SIZE+1];
    int logged_in_status;

    //this flags helps to tell whether the client is ready to disconnect after failing to login
    //As server has to wait for one second before closing the FIFO and unlink
    int ready_disconnection; 

    struct timespec disconnect_time;//This is used to compare the current time and tell whether should disconnect the client

    read_line_buffer reader_buffer;//This will help to store the remaining message,if read_message() not reading a complete message

    pthread_mutex_t lock;//This is used to prevent race condition for sequence number allocation and other potential race conditions
    response_entry* response_head;

    //This represents the next ready-to-send response number
    uint64_t response_sequence_counter; //This will only increment by 1 when a response is sent
    uint64_t request_sequence_counter;//This will increment by 1 when a new task needs to reply
    int remaining_requests; //This prevent from unsolving task being deleted when client disconnects
    int closing_status;


    uint64_t next_apply_num; //This will only be used for requests that cannot be done in paralle
    uint64_t next_apply_to_run_num; //This indicates the next non-parallel request,which allows to be processed
    pthread_cond_t apply_cond_var;
} client_entry;

typedef struct response_entry{
    client_entry* client;
    uint64_t sequence_num; //Considering that there are a lot of response, then the data type must be large enough.
    char response[MAX_RESPONSE_SIZE];
    struct response_entry* next;
}response_entry;



typedef struct request_entry{
    client_entry* client;
    uint64_t response_num; //Considering that there are a lot of response, then the data type must be large enough.
    uint64_t process_num; //This is used  to tell the order of non-parallel commands
    int is_non_parallel; //A flag telling whether this request can be done in parallel
    char command_line[INPUT_MAX_SIZE];
    struct request_entry* next;
    uint64_t wait_until_apply_num; //This is used for generate
}request_entry;


typedef struct request_pool{
    request_entry* head; 
    request_entry* tail;
    pthread_mutex_t lock;
    pthread_cond_t new_work;
    int closed_status;
}request_pool;

#endif