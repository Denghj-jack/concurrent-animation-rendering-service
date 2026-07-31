#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_response.h"
#include "common_io_use.h"
#include "configuration.h"


void print_rpc_response(char* reply_message){
    if(reply_message == NULL){
        return;
    }
    size_t len = strlen(reply_message);
    if(len == 0){
        return;
    }
    reply_message[len-1] = '\0';
    char* last_point = NULL;
    char* num = strtok_r(reply_message , " " , &last_point);
    if(num == NULL){
        return;
    }
    if(strcmp(num , "-1") == 0){
        printf("RPC Failed\n");
    }
    else if(strcmp(num , "-2") == 0){
        printf("Value error\n");
    } 
    else if(strcmp(num , "-3") == 0){
        printf("Internal error\n");
    }
    else if(strcmp(num , "0") == 0){
        char* second_token = strtok_r(NULL , " " , &last_point);
        char* third_token = strtok_r(NULL , " " , &last_point);
        if(second_token == NULL){
            printf("Success\n");
        }
        else if(third_token == NULL){
            if(strcmp(second_token , "-1") == 0){
                printf("Data write failed\n");
            }
            else{
                printf("Success %s\n" , second_token);
            }
        }
        else{
            if(strcmp(third_token , "-1") == 0){
                printf("Movie write failed\n");
            }
            else if(strcmp(third_token , "0") == 0){
                printf("Success\n");
            }
        }
    }
    fflush(stdout);
    return;
}



int handler_one_server_reply(client_info* client_state , char* reply){
    if(client_state -> waiting_login_response == 1){
        client_state -> waiting_login_response = 0;
        if(strncmp(reply , "Reject" , 6) == 0){
            printf("%s" , reply);
            fflush(stdout);
            return 1;
        }
        char* last_point = NULL;
        long balance =strtol(reply , &last_point , 10);
        if(balance <= 0){ //The client has already login, but it sent the login command again
            print_rpc_response(reply);
            return 0;
        }

        printf("Welcome %s. Your balance is %ld\n" , client_state->username , balance);
        fflush(stdout);
        client_state-> logged_in = 1;
        return 0;
    }
    else{//This will output the result for RPC commands
        print_rpc_response(reply);
        return 0;
    }
       
}


int handle_server_reponse(client_info* client_state){
    while(1){
        char reply[INPUT_MAX_SIZE];
        int read_status = read_message(client_state ->s2c_fd , &client_state->s2c_buffer , reply);
        if(read_status == 0){
            if(handler_one_server_reply(client_state , reply) == 1){//login got rejected
                return 1;
            }
            else{
                continue;
            }
        }
        if(read_status == 1){
            return 0;
        }
        if(read_status == 2){
            return 1;
        }
        if(read_status == 3){
            perror("Failed to read from server\n");
            return 1;
        }
    }
}