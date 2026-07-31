#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include "common_io_use.h"
#include "client_connection.h"
#include "client_struct_types.h"
#include "client_response.h"



int handler_stdin_event(client_info* state){
    //These few strings are used to prevent from strtok_r() modifying the original string
    char stdin_input[INPUT_MAX_SIZE];
    char parse_copy[INPUT_MAX_SIZE];
    char send_copy[INPUT_MAX_SIZE];
    if(fgets(stdin_input , sizeof(stdin_input) , stdin) == NULL){
        send_message(state->c2s_fd , "Disconnect\n");
        return 1;
    }
    strncpy(send_copy , stdin_input , sizeof(send_copy)-1);
    send_copy[sizeof(send_copy) - 1] = '\0';


    strncpy(parse_copy , stdin_input , sizeof(parse_copy)-1);
    parse_copy[sizeof(parse_copy) - 1] = '\0';

    char* last_point = NULL;
    char* command = strtok_r(parse_copy, " \t\r\n" , &last_point);
    if(command == NULL){
        return 0;
    }
    if(strcmp(command , "Disconnect") == 0){
        send_message(state->c2s_fd ,send_copy);
        return 1;
    }
    if(state -> logged_in == 0){//Not login
        if (strcmp(command , "Login") == 0){
            if(state -> waiting_login_response == 1){
                printf("Not logged in\n");
                fflush(stdout);
                return 0;
            }
        char* input_username = strtok_r(NULL ,  " \t\r\n" , &last_point);

        if(input_username != NULL){
            strncpy(state->username , input_username , USERNAME_MAX_SIZE);
            state->username[USERNAME_MAX_SIZE] = '\0';
        }
        else{
            state -> username[0] = '\0';
        }

        if(send_message(state->c2s_fd , send_copy)  != 0){
            return 1;
        }
        state ->waiting_login_response = 1;
        return 0;
    }
    else{//invalid input without login
        printf("Not logged in\n");
        fflush(stdout);
        return 0;
    }
    }


    if(send_message(state->c2s_fd , send_copy) == -1){//This represents the client already login, and it's sending RPC commands
        return 1;
    }
    return 0;
    
}


int client_event_poll(client_info* state){
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = state -> s2c_fd; //hear from the server
    fds[1].events = POLLIN | POLLHUP | POLLERR;
    while(1){
        fds[0].revents = 0;
        fds[1].revents = 0;
        int ready_fds = poll(fds , 2 , -1);
        if(ready_fds == -1){
            if(errno == EINTR){
                continue;
            }
            else{
                perror("Poll is broken\n");
                return 1;
            }
        }
        //message from the server
        if((fds[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0){
            if(handle_server_reponse(state) == 1){
                return 0;
            }
        }
        //new client connection
        if((fds[0].revents & POLLIN) != 0){
            if(handler_stdin_event(state) == 1){
                return 0;
            }
        }
    }
}


int main(int argc, char** argv) {
    if(argc != 2){
        fprintf(stderr , "Please input two command line arguments exactly");
        return 1;
    }
    char* end = NULL;
    long value;
    errno = 0;
    value = strtol(argv[1] , &end , 10); //strtol is thread-safe
    if(end == argv[1] || *end != '\0' || value <= 0){
        fprintf(stderr , "Invalid server pid");
        return 1;
    }
    pid_t server_pid = (pid_t)value;
    client_info state;
    memset(&state , 0 , sizeof(state));
    state.c2s_fd = -1; //this helps to distinguish from whether this client has connected to the server
    state.s2c_fd = -1;
    if(connection_to_server(server_pid , &state.c2s_fd , &state.s2c_fd) != 0){
        return 1;
    }
    int poll_flag = client_event_poll(&state);

    if(state.c2s_fd != -1){
        close(state.c2s_fd);
    }
    if(state.s2c_fd != -1){
        close(state.s2c_fd);
    }
    return poll_flag;

}

