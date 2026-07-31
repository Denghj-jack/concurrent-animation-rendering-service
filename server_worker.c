#include "server_worker.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "common_io_use.h"
#include "configuration.h"
#include "server_dispatch_work.h"
#include "rpc_preprocess.h"


request_pool big_request_pool;


int big_request_pool_init(request_pool* pool){
    if(pool == NULL){
        return -1;
    }
    pool ->head = NULL;
    pool -> tail = NULL;
    if(pthread_mutex_init(&pool->lock , NULL) != 0){
        return -1;
    }
    //This condition variable will wake up the workers, when the requet pool is empty before
    if(pthread_cond_init(&pool ->new_work , NULL) != 0){
        pthread_mutex_destroy(&pool ->lock);
        return -1;
    }
    pool->closed_status = 0;
    return 0;
}


static int is_create_command(char* commands){
    if(commands == NULL){
        return 0;
    }
    if(strcmp(commands , "create_canvas") == 0){
        return 1;
    }
    if(strcmp(commands , "create_sprite") == 0){
        return 1;
    }
    if(strcmp(commands , "create_rectangle") == 0){
        return 1;
    }
    if(strcmp(commands , "create_circle") == 0){
        return 1;
    }
    return 0;
}

//This function is used to check whether this request can be done in parallel
int request_process_in_order(char* line){
    char copy_command[INPUT_MAX_SIZE];
    if(line == NULL){
        return 1;
    }

    strncpy(copy_command , line , sizeof(copy_command)-1);
    copy_command[sizeof(copy_command) - 1] = '\0';
    char* last_point = NULL;
    char* command = strtok_r(copy_command , " \n" , &last_point);
    if(command == NULL){
        return 1;
    }
    if(strcmp(command , "generate") == 0 || is_create_command(command) == 1){
        return 0;
    }
    return 1;
}

int insert_request_into_pool(request_pool* pool , request_entry* request){
    if(pool == NULL || request == NULL){
        return -1;
    }
    request -> next = NULL;
    pthread_mutex_lock(&pool -> lock);
    if(pool -> tail == NULL){//This indicates that there is no task in the pool before it
        pool -> head = request;
        pool-> tail = request;
    }
    else{
        pool -> tail -> next = request;
        pool -> tail = request;
    }
    pthread_cond_signal(&pool -> new_work);
    pthread_mutex_unlock(&pool -> lock);
    return 0;
}


request_entry* pop_out_request_from_pool(request_pool* pool){
    if(pool == NULL){
        return NULL;
    }
    request_entry* request;
    pthread_mutex_lock(&pool ->lock);
    while(pool ->head == NULL && pool -> closed_status == 0){
        pthread_cond_wait(&pool-> new_work , &pool ->lock);
    }
    if(pool -> head == NULL && pool -> closed_status == 1){
        pthread_mutex_unlock(&pool ->lock);
        return NULL;
    }
    request = pool-> head;
    pool -> head = request -> next;
    if(pool -> head == NULL){
        pool -> tail = NULL;
    }
    pthread_mutex_unlock(&pool -> lock);
    return request;
}


int insert_reponse_into_sorted_ls(client_entry* client , response_entry* entry){
    if(client == NULL || entry == NULL){
        return -1;
    }
    response_entry* current = client -> response_head;
    response_entry* previous = NULL;
    //Sequence number helps to locate the position in the response linked list
    while(current != NULL && current ->sequence_num < entry->sequence_num){
        previous = current;
        current = current -> next;
    }
    if(previous == NULL){
        entry -> next = client -> response_head;
        client -> response_head = entry;
    }
    else{
        entry -> next = current;
        previous -> next  =entry;
    }
    return 0;
}


void update_client_response_queue(client_entry* client){
    if(client == NULL){
        return;
    }
    response_entry* entry;
    //Time to send the response back the client
    while(client -> response_head != NULL && client->response_head -> sequence_num == client -> response_sequence_counter){
        entry = client -> response_head;
    
    if(client ->closing_status == 0){
        send_message(client->s2c_fd , entry ->response);
    }
    client -> response_sequence_counter++;
    client -> response_head = entry -> next;
    free(entry);
    entry = NULL;
    }
}

//This function is called when the worker thread finishes the task(including something wrong with the memory allocation)
void complete_request(client_entry* client , uint64_t sequence_number , char* response_message){
    if(client == NULL || response_message == NULL){
        return;
    }
    response_entry* entry = (response_entry*)calloc(1 , sizeof(response_entry));

    pthread_mutex_lock(&client -> lock);
    client -> remaining_requests--;
    if(entry == NULL){
        pthread_mutex_unlock(&client -> lock);
        return;
    }
    if(client -> closing_status == 1){
        free(entry);
        pthread_mutex_unlock(&client -> lock);
        return;
    }

    entry ->sequence_num = sequence_number;
    entry ->client = client;
    entry -> next = NULL;
    strncpy(entry ->response , response_message , sizeof(entry ->response)-1);
    entry ->response[sizeof(entry -> response) -1] = '\0';
    
    insert_reponse_into_sorted_ls(client , entry);
    update_client_response_queue(client);
    pthread_mutex_unlock(&client -> lock);
}

//This function is used to insert the task from client into the big task pool
int insert_request_for_client(client_entry* client , char* line){
    if(client == NULL || line == NULL){
        return -1;
    }

    request_entry* request = (request_entry*)calloc(1 , sizeof(request_entry));
    if(request == NULL){
        return -1;
    }
    strncpy(request ->command_line , line , sizeof(request -> command_line) -1);
    request -> command_line[sizeof(request -> command_line) -1] = '\0';
    request -> client = client;
    request -> next = NULL;

    pthread_mutex_lock(&client -> lock);
    if(client -> closing_status == 1){
        pthread_mutex_unlock(&client -> lock);
        free(request);
        request = NULL;
        return -1;
    }
    request -> response_num = client -> request_sequence_counter;
    client -> request_sequence_counter++;
    request -> is_non_parallel =  request_process_in_order(line);
    if(request -> is_non_parallel == 1){
        request ->process_num = client -> next_apply_num;
        client -> next_apply_num++;
    }
    else{
        request -> process_num = 0;
        request -> wait_until_apply_num = client -> next_apply_num;
    }
    client -> remaining_requests++;
    
    pthread_mutex_unlock(&client -> lock);

    if(insert_request_into_pool(&big_request_pool , request) == -1){
        complete_request(client , request -> response_num , "-3\n");
        free(request);
        request = NULL;
        return -1;
    }

    return 0;
}



void* worker_function(void* arg){
    (void)arg;
    while(1){
        request_entry* request = pop_out_request_from_pool(&big_request_pool);
        if(request == NULL){//This indicates that the request pool is closed
            break;
        }
        client_entry* client = request -> client;

        if(request ->is_non_parallel == 1){
            //This corresponds to the request which will modify the shared resources
            pthread_mutex_lock(&client -> lock);

            while(client -> closing_status == 0 && request -> process_num != client -> next_apply_to_run_num){
                pthread_cond_wait(&client -> apply_cond_var , &client -> lock);
            }//sleep until request -> process_num == client -> next_apply_to_run_num
            if(client -> closing_status == 1){
                client -> remaining_requests--;//This is a flag helping to processing othere working requests
                if(request -> process_num == client -> next_apply_to_run_num){
                    client -> next_apply_to_run_num++;
                }
                //This will notify all the other worker threads: "This client is died"
                pthread_cond_broadcast(&client -> apply_cond_var);

                pthread_mutex_unlock(&client -> lock);
                free(request);
                request = NULL;
                continue;
            }
            pthread_mutex_unlock(&client -> lock);
            //After unlock, this will start process this request
        }


        else{//Allows for parallel tasks(generatation)
            pthread_mutex_lock(&client -> lock);
            //The variable "wait_until_apply_num" is used to tell when will the non-parallel commands finish
            while(client -> closing_status == 0 && client -> next_apply_to_run_num < request -> wait_until_apply_num){
                pthread_cond_wait(&client -> apply_cond_var , &client -> lock);
            }
            if(client -> closing_status == 1){
                client -> remaining_requests--;
                pthread_mutex_unlock(&client -> lock);
                free(request);
                request = NULL;
                continue;
            }
            //After unlock, this will start process this request
            pthread_mutex_unlock(&client -> lock);
        }


        char response[MAX_RESPONSE_SIZE];
        run_rpc_command(client , request -> command_line , response , sizeof(response));
        if(request-> is_non_parallel == 1){
            pthread_mutex_lock(&client->lock);
            if(request->process_num == client->next_apply_to_run_num){
                client->next_apply_to_run_num++;
            }
            //Once this non-parallel command is finished, this will notify two groups of requests
            //One is still the non-parallel commands, telling the next command: "you can work now"
            //The other one is to see whether the generation() can be processed 
            pthread_cond_broadcast(&client->apply_cond_var);
            pthread_mutex_unlock(&client->lock);
        }
        complete_request(client , request -> response_num , response);
        free(request);
    }
    return NULL;
}