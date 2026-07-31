#ifndef SERVER_WORKER_H
#define SERVER_WORKER_H

#include <stdint.h>
#include "server_struct_types.h"

extern request_pool big_request_pool; //This is used to collect all tasks from all users

int big_request_pool_init(request_pool* pool);

//This function checks whether a request should be processed in order(used for generate)
//As generate does not modify the canvas, and we can allow several generation at the same time
int request_process_in_order(char* line);


//This will only insert the given request entry into the request pool
int insert_request_into_pool(request_pool* pool, request_entry* request);

request_entry* pop_out_request_from_pool(request_pool* pool);


//complete_request() -> insert_response_into_sorted_ls() -> update_client_response_queue()

//Insert the response into the client's sorted response queue
int insert_reponse_into_sorted_ls(client_entry* client, response_entry* entry);

//This function will check the head of client's response queue
//If the client->response_head -> sequence_num == client -> response_sequence_counter
//then we send the response out, and update the response queue head
void update_client_response_queue(client_entry* client);

//This function is doing some postprocessing work after solving the request, but before responding back to the client
//decrement the client's remaining_request
//create a response entry and insert it into client's response queue
//and finally sending out the response using update_clien_response_queue() function
void complete_request(client_entry* client , uint64_t sequence_number , char* response_message);


//This will create a request entry first, and then insert into the request pool
int insert_request_for_client(client_entry* client, char* line);

//This is used for pthread_create() fucntion
void* worker_function(void* arg);

#endif