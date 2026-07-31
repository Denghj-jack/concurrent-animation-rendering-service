#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "server_worker.h"
#include "server_connection.h"
#include "server_resources.h"



int main(int argc, char** argv) {
    if(argc != 2){
        fprintf(stderr , "Please input two command line arguments exactly");
        return 1;
    }

    char* end = NULL;
    long value;
    errno = 0;
    value = strtol(argv[1] , &end , 10);
    if(end == argv[1] || *end != '\0' || value <= 0){
        fprintf(stderr , "Invalid thread size");
        return 1;
    }
    long thread_size = (long)value;
    if(big_request_pool_init(&big_request_pool) == -1){
        return 1;
    }
    pthread_t* worker_head = (pthread_t*)calloc((size_t)thread_size , sizeof(pthread_t));
    if(worker_head == NULL){
        pthread_mutex_destroy(&big_request_pool.lock);
        pthread_cond_destroy(&big_request_pool.new_work);
        return 1;
    }
    
    //Signal sending from the client pretends to be the message sending from the pipe
    if(create_pipes_for_signal() == -1){
        free(worker_head);
        close_signal_pipes();
        pthread_mutex_destroy(&big_request_pool.lock);
        pthread_cond_destroy(&big_request_pool.new_work);
        return 1;
    }
    if(register_signal_handlers() == -1){
        free(worker_head);
        close_signal_pipes();
        pthread_mutex_destroy(&big_request_pool.lock);
        pthread_cond_destroy(&big_request_pool.new_work);
        return 1;
    }

    for(long num = 0 ; num < thread_size ; num++){
        if(pthread_create(&worker_head[num] , NULL , &worker_function , NULL) != 0){
            free(worker_head);
            close_signal_pipes();
            pthread_mutex_destroy(&big_request_pool.lock);
            pthread_cond_destroy(&big_request_pool.new_work);
            return 1;
        }
    }

    pid_t pid = getpid();
    printf("Server PID: %d\n" , pid);
    fflush(stdout); 

    server_event_poll(); //initialise the poll for monitoring the client behaviours

    pthread_mutex_lock(&big_request_pool.lock);
    big_request_pool.closed_status = 1;
    pthread_cond_broadcast(&big_request_pool.new_work);
    pthread_mutex_unlock(&big_request_pool.lock);
    for(long num = 0 ; num < thread_size ; num++){
        pthread_join(worker_head[num] , NULL);
    }
    free(worker_head);
    request_entry* remaining_request_entry = big_request_pool.head;
    request_entry* next = NULL;
    while(remaining_request_entry != NULL){
        next = remaining_request_entry ->next;
        free(remaining_request_entry);
        remaining_request_entry = next;
    }
    big_request_pool.head = NULL;
    big_request_pool.tail = NULL;
    delete_all_service_resources();
    delete_all_animate_resources();
    pthread_mutex_destroy(&big_request_pool.lock);
    pthread_cond_destroy(&big_request_pool.new_work);
    close_signal_pipes();
    

    return 0;
}