#include "server_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <stdbool.h>
#include "libanimate/include/animate/animate.h"
#include "configuration.h"
#include "common_io_use.h"
#include "rpc_preprocess.h"
#include "authentication.h"
#include "server_worker.h"
#include "server_resources.h"



int signal_pipe_fds[2] = {-1 , -1}; //-1 helps to identify whether the fd is initialised
pthread_mutex_t client_head_lock = PTHREAD_MUTEX_INITIALIZER;
client_entry* client_head = NULL;
struct pollfd poll_fds[MAX_POLL_FDS];
client_entry* poll_clients[MAX_POLL_FDS];
static volatile sig_atomic_t server_quit_flag = 0;


int count_connected_canvas_users(canvas_entry* canvas){
    if(canvas == NULL){
        return -1;
    }
    int connected_users_num = 0;
    client_entry* current = client_head;
    while(current != NULL){
        if(current -> logged_in_status == 1 && current -> closing_status == 0 && user_access_canvas_check(canvas , current -> username) == 1){
            connected_users_num++;
        }
        current = current -> next;
    }
    return connected_users_num;
}


void sigusr1_handler(int signal , siginfo_t* info , void* context){
    (void)signal;
    (void)context;
    if(info != NULL){
        pid_t client_pid = info -> si_pid;
        ssize_t num_bytes = write(signal_pipe_fds[1] , &client_pid , sizeof(client_pid));
        if(num_bytes != sizeof(client_pid)){
            return;
        }
    }
    return;
}

static void shutdown_handler(int signal){
    (void)signal;
    server_quit_flag =1;
}


int create_pipes_for_signal(void){
    if(pipe(signal_pipe_fds) == -1){
        return -1;
    }
    if(set_non_blocking(signal_pipe_fds[0]) == -1 || set_non_blocking(signal_pipe_fds[1]) == -1){
        return -1;
    }
    return 0;
}


int register_signal_handlers(void){
    struct sigaction sa;
    sa.sa_sigaction = &sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1 , &sa , NULL) == -1){
        return -1;
    }

    struct sigaction sa_pipe;
    sa_pipe.sa_handler = SIG_IGN;
    sigemptyset(&sa_pipe.sa_mask);
    sa_pipe.sa_flags = 0;
    if(sigaction(SIGPIPE , &sa_pipe , NULL) == -1){//This prevents from shutting down the server when sending message to disconnected client 
        return -1;
    }

    struct sigaction sa_shutdown;
    sa_shutdown.sa_flags =0;
    sa_shutdown.sa_handler = &shutdown_handler;
    sigemptyset(&sa_shutdown.sa_mask);
    if (sigaction(SIGINT , &sa_shutdown , NULL) == -1){
        return -1;
    }
    if (sigaction(SIGTERM , &sa_shutdown , NULL) == -1){
        return -1;
    }
    return 0;

}



int create_named_pipes(char* fifo_C2S_path , char* fifo_S2C_path){
    unlink(fifo_C2S_path);
    unlink(fifo_S2C_path);
    if(mkfifo(fifo_C2S_path , 0666) == -1){
        perror("Failed to set up the named pipe C2S.\n");
        return -1;
    }
    if(mkfifo(fifo_S2C_path , 0666) == -1){
        unlink(fifo_C2S_path);
        perror("Failed to set up the named pipe S2C.\n");
        return -1;
    }
    return 0;
}


void add_client(client_entry** client_head , client_entry* adding_client){
    adding_client -> next = *client_head;
    *client_head = adding_client;
    return;
}


void delete_reponse_queue(response_entry* response_head){
    response_entry* current = response_head;
    response_entry* next = NULL;
    while(current != NULL){
        next = current -> next;
        free(current);
        current = next;
    }   
    return;
}


void free_client(client_entry* client){
    if(client == NULL){
        return;
    }
    if(client -> c2s_fd != -1){
        close(client->c2s_fd);
    }
    if(client -> s2c_fd != -1){
        close(client->s2c_fd);
    }
    unlink(client -> fifo_c2s_path);
    unlink(client -> fifo_s2c_path);
    delete_reponse_queue(client -> response_head);
    pthread_cond_destroy(&client -> apply_cond_var);
    pthread_mutex_destroy(&client -> lock);
    free(client);
    client = NULL;
}



void mark_client_should_close(client_entry* client){
    if(client == NULL){
        return;
    }
    pthread_mutex_lock(&client ->lock);

    client -> closing_status = 1;
    delete_reponse_queue(client -> response_head);
    client -> response_head = NULL;
    pthread_cond_broadcast(&client -> apply_cond_var);
    pthread_mutex_unlock(&client -> lock);


    pthread_mutex_lock(&canvas_lock);
    canvas_entry* current = canvas_head;
    while(current != NULL){
        pthread_cond_broadcast(&current ->barrier_cond);
        current = current ->next;
    }
    pthread_mutex_unlock(&canvas_lock);

    if(client -> c2s_fd != -1){
        close(client -> c2s_fd);
        client -> c2s_fd = -1;
    }
}


void delete_died_user_resources(char* username){
    if(username == NULL){
        return;
    }

    pthread_mutex_lock(&canvas_lock);
    pthread_mutex_lock(&placement_lock);
    pthread_mutex_lock(&sprite_lock);

    placement_entry* current_placement = placement_head;
    placement_entry* next_placement = NULL;
    while(current_placement != NULL){
        next_placement = current_placement -> next;
        if(strcmp(username , current_placement ->owner_username) == 0 && current_placement ->active_status == 1){
            animate_destroy_placement((struct sprite_placement*) current_placement -> placement_pointer);
            remove_placement_inactive(current_placement ->id);
        }
        current_placement = next_placement;
    }


    canvas_entry* current_canvas = canvas_head;
    canvas_entry* next_canvas = NULL;
    while(current_canvas != NULL){
        next_canvas = current_canvas -> next;
        if(strcmp(username , current_canvas ->owner_username) == 0 && current_canvas ->active_status == 1 && count_connected_canvas_users(current_canvas) == 0){
            animate_destroy_canvas((struct canvas*) current_canvas -> canvas_pointer);
            delete_placements_in_inactive_canvas(current_canvas -> id);
            remove_canvas_inactive(current_canvas ->id);
        }
        current_canvas = next_canvas;
    }


    sprite_entry* current_sprite = sprite_head;
    sprite_entry* next_sprite = NULL;
    while(current_sprite != NULL){
        next_sprite = current_sprite -> next;
        if(strcmp(username , current_sprite ->owner_username) == 0 && current_sprite ->active_status == 1){
            bool result = animate_destroy_sprite((struct sprite*) current_sprite -> sprite_pointer);
            if(result == 0){
                remove_sprite_inactive(current_sprite ->id);
            }
        }
        current_sprite = next_sprite;
    }


    pthread_mutex_unlock(&sprite_lock);
    pthread_mutex_unlock(&placement_lock);
    pthread_mutex_unlock(&canvas_lock);
    
}



void delete_closed_clients(void){
    client_entry* current = client_head;
    client_entry* next = NULL;
    while(current != NULL){
        next = current -> next;
        pthread_mutex_lock(&current->lock);
        int check_free_status = 0;//This is used to check whether there are some requsts or response in the queue
        if(current -> closing_status == 1 && current -> remaining_requests == 0 && current ->response_head == NULL){
            check_free_status = 1;
        }
        pthread_mutex_unlock(&current -> lock);
        if(check_free_status == 1){
            delete_died_user_resources(current -> username);
            pthread_mutex_lock(&client_head_lock);
            delete_client_from_linkedlist(&client_head , current);
            pthread_mutex_unlock(&client_head_lock);
        }
        current = next;
    }
}


void delete_client_from_linkedlist(client_entry** client_head , client_entry* delete_client){
    client_entry* previous_client = NULL;
    client_entry* current = *client_head;
    while(current != NULL){
        if(current == delete_client){
            if(previous_client == NULL){
                *client_head = current->next;
            }
            else{
                previous_client -> next = current -> next;
            }
            free_client(current);
            return;
        }
        previous_client = current;
        current = current->next;
    }
    return;
}



int user_authentication(client_entry* client , const char* username){
    long user_balance = 0;
    if(username == NULL){
        send_message(client->s2c_fd , "Reject UNAUTHORISED\n");
        return -1;
    }
    if(check_username(username) == 1){
        send_message(client->s2c_fd , "Reject UNAUTHORISED\n");
        return -1;
    }
    int status = search_user_info(username , &user_balance);
    if(status <= 0){
        send_message(client->s2c_fd , "Reject UNAUTHORISED\n");
        return -1;
    }
    if(user_balance <= 0){
        send_message(client->s2c_fd , "Reject BALANCE\n");
        return -1;
    }
    else{
        char message[128];
        snprintf(message , sizeof(message) , "%ld\n" , user_balance);
        send_message(client->s2c_fd , message);
        strncpy(client->username , username , USERNAME_MAX_SIZE);
        client->username[USERNAME_MAX_SIZE] = '\0';
        client -> logged_in_status = 1;
        return 0;
    }
}



client_entry* allocate_resource_new_client(pid_t current_client_pid){
    client_entry* new_client;
    new_client = (client_entry*)calloc(1 , sizeof(client_entry));
    if(new_client == NULL){
        return NULL;
    }
    new_client ->pid = current_client_pid;
    new_client ->c2s_fd = -1;//Initialized value for file descriptor
    new_client -> s2c_fd = -1;
    new_client ->logged_in_status = 0;
    new_client ->closing_status = 0;
    new_client ->remaining_requests = 0;
    new_client -> request_sequence_counter = 0;
    new_client -> response_sequence_counter = 0;
    new_client -> response_head = NULL;
    new_client -> next_apply_num = 0;
    new_client -> next_apply_to_run_num = 0;
    if(pthread_mutex_init(&new_client-> lock , NULL) != 0){
        free(new_client);
        return NULL;
    }
    if(pthread_cond_init(&new_client->apply_cond_var , NULL) != 0){
        pthread_mutex_destroy(&new_client-> lock);
        free(new_client);
        new_client = NULL;
        return NULL;
    }
    return new_client;
}


client_entry* create_connection(pid_t current_client_pid){
    client_entry* new_client = allocate_resource_new_client(current_client_pid);
    if(new_client == NULL){
        return NULL;
    }

    snprintf(new_client ->fifo_c2s_path , PIPE_PATH_MAX_SIZE , "FIFO_C2S_%d" , current_client_pid);
    snprintf(new_client ->fifo_s2c_path , PIPE_PATH_MAX_SIZE , "FIFO_S2C_%d" , current_client_pid);
    if(create_named_pipes(new_client ->fifo_c2s_path  , new_client ->fifo_s2c_path) == -1){
        free_client(new_client);
        return NULL;
    }

    if(kill(current_client_pid , SIGUSR2) == -1){
        perror("Failed to send out the SIGUSR2 signal to client");
        free_client(new_client);
        return NULL;
    }

    new_client->c2s_fd = open(new_client ->fifo_c2s_path , O_RDONLY | O_NONBLOCK); //Just in case the client has not yet opened
    if(new_client->c2s_fd == -1){
        perror("failed to open the named pipe from client to server");
        free_client(new_client);
        return NULL;
    }
    new_client ->s2c_fd = open(new_client ->fifo_s2c_path , O_RDWR); //Just in case the client has not yet opened
    if(new_client ->s2c_fd == -1){
            perror("failed to open the named pipe from server to client");
            free_client(new_client);
            return NULL;
    }

    return new_client;
}



int handle_one_client_command(client_entry* client , char* line){
    rpc_commands command;
    memset(&command , 0 , sizeof(command));
    //failed to split the commands
    if(split_rpc_commands(line , &command) == -1){
        send_message(client -> s2c_fd , "-1\n");
        return 0;
    }
    if(command.token_count == 0){
        return 0;
    }
    if(strcmp(command.tokens[0] , "Disconnect") == 0){
        if(command.token_count != 1){//more than one argument
            if(client -> logged_in_status == 1){
                insert_request_for_client(client , line);
            }
            else{
                send_message(client -> s2c_fd , "-1\n");
            }
            return 0;
        }
        return 1;
    }
    if(strcmp(command.tokens[0] , "Login") == 0){
        if(client -> logged_in_status == 1 || command.token_count != 2){
            send_message(client -> s2c_fd , "-1\n");
            return 0;
        }
        if(user_authentication(client , command.tokens[1]) == -1){
            client ->ready_disconnection = 1;
            if(clock_gettime(CLOCK_MONOTONIC , &client ->disconnect_time) == -1){
                return 1;
            }
            client -> disconnect_time.tv_sec += 1;
            return 0;
        }
        return 0;
    }
    if(client ->logged_in_status == 0){//Illegal input, without login first
        send_message(client->s2c_fd , "-1\n");
        return 0;
    }
    //if this command is valid and the user has logged in, then we process the request
    insert_request_for_client(client , line);
    return 0;
}




int handle_client_message(client_entry* client){
    char single_message[INPUT_MAX_SIZE];

    while(1){
    int reading_status = read_message(client->c2s_fd , &client ->reader_buffer , single_message);
    if(reading_status == 0){
        int disconnect_flag = handle_one_client_command(client , single_message);
        if(disconnect_flag == 1){
            return 1;
        }
        continue;
    }
    if(reading_status == 1){//no message to read, but still connected
        return 0; 
    }
    if(reading_status == 2){//the client is disconncted
        return 1;
    }
    if(reading_status == 3){
        perror("Failed to read from client\n");
        return 1;
        }
    }
}


void accept_clients_via_pipes(void){
    while(1){
    pid_t client_pid;
    ssize_t read_num = read(signal_pipe_fds[0] , &client_pid , sizeof(client_pid));
    if (read_num == (ssize_t) sizeof(client_pid)){
        client_entry* new_client = create_connection(client_pid);
        if(new_client == NULL){
            continue;
        }

        pthread_mutex_lock(&client_head_lock);
        add_client(&client_head , new_client);
        pthread_mutex_unlock(&client_head_lock);

        continue;
        }
    if(read_num == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
        break;
    }
    if(read_num == -1 && errno == EINTR){
        continue;
    }
    break;
    }
    return;
}



nfds_t update_poll(void){
    nfds_t num_fds = 0;
    poll_fds[0].fd = signal_pipe_fds[0];
    poll_fds[0].events = POLLIN;
    poll_fds[0].revents = 0;
    poll_clients[num_fds] = NULL;
    num_fds++;

    pthread_mutex_lock(&client_head_lock);
    client_entry* current = client_head;
    while(current != NULL && num_fds < MAX_POLL_FDS){
        if(current->c2s_fd != -1 && current ->ready_disconnection != 1){
            poll_clients[num_fds] = current;
            poll_fds[num_fds].fd = current->c2s_fd;
            poll_fds[num_fds].events = POLLIN;
            poll_fds[num_fds].revents = 0;
            num_fds++;
        }
        current = current -> next;
    }
    pthread_mutex_unlock(&client_head_lock);
    return num_fds;
}


void delete_all_service_resources(void){
    client_entry* current = client_head;
    while(current != NULL){
        client_entry* next = current -> next;
        free_client(current);
        current = next;
    }
    client_head = NULL;
}




int time_over_check(const struct timespec disconnect_time){
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC , &current_time);
    if(current_time.tv_sec > disconnect_time.tv_sec){
        return 1;
    }
    else if(current_time.tv_sec == disconnect_time.tv_sec && current_time.tv_nsec >= disconnect_time.tv_nsec){
        return 1;
    }
    return 0;
}


void delete_timeout_clients(void){
    client_entry* current = client_head;
    client_entry* next;
    while(current != NULL){
        next = current -> next;
        if(current -> ready_disconnection && time_over_check(current ->disconnect_time) == 1){

            pthread_mutex_lock(&client_head_lock);
            delete_client_from_linkedlist(&client_head , current);
            pthread_mutex_unlock(&client_head_lock);
        }
        current = next;
    }
}


void close_signal_pipes(void){
    if(signal_pipe_fds[0] != -1){
        close(signal_pipe_fds[0]);
        signal_pipe_fds[0] = -1;
    }
    if(signal_pipe_fds[1] != -1){
        close(signal_pipe_fds[1]);
        signal_pipe_fds[1] = -1;
    }
    return;
}



int server_event_poll(void){
        while(server_quit_flag == 0){
        nfds_t num_polls = update_poll();
        int poll_status = poll(poll_fds , num_polls , 50);

        if(poll_status == -1){
            if(errno == EINTR){//new clients sends a signal to the server
                accept_clients_via_pipes();
                delete_timeout_clients();
                continue;
            }
            else{
                break;
            }
        }
        else if(poll_status > 0){
        if(poll_fds[0].revents & POLLIN){
            accept_clients_via_pipes();
        }
        for(nfds_t num = 1 ; num < num_polls ; num++){
            client_entry* current_client = poll_clients[num];
            if ((poll_fds[num].revents & (POLLIN | POLLHUP | POLLERR)) != 0){
                if(handle_client_message(current_client) == 1){
                    mark_client_should_close(current_client);
                    delete_closed_clients();
                }
            }
        }
    }
    delete_timeout_clients();
    delete_closed_clients();
        }
        return 0;
}