#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include "animate_tables.h"
#include "server_struct_types.h"

extern pthread_mutex_t client_head_lock;
extern client_entry* client_head;

//This function is used for barrier() and delete the canvas(whether there's only one client
//left who can access the canvas
int count_connected_canvas_users(canvas_entry* canvas);
void sigusr1_handler(int signal , siginfo_t* info , void* context);

//I created a pipe for handling new client joining,as I can then add this pipe into my poll() for listening
int create_pipes_for_signal(void);
int register_signal_handlers(void);
int create_named_pipes(char* fifo_C2S_path , char* fifo_S2C_path);

//This is used for adding client to my client linked list, where the head is the newest client
void add_client(client_entry** client_head , client_entry* adding_client);

//this is used to cleanup the resource when I delete the corresponding client or the server is closed
void delete_reponse_queue(response_entry* response_head);

//This is used to cleanup the client struct resources, including the mutex and condition variable
void free_client(client_entry* client);

//This function is very important, as this prevents interrupting
//the worker thread from still processsing this client's request, which may result in use-after-free
void mark_client_should_close(client_entry* client);

//This function is used for deleting the client-related resources, such as owning placement, sprite and canvas
void delete_died_user_resources(char* username);


//This function is used to delete the client from the client linked list, as well as the animation resources
void delete_closed_clients(void);

//This function is used to delete the client from the client linked list
void delete_client_from_linkedlist(client_entry** client_head , client_entry* delete_client);

//This is used to check whether this client can be joined the server
int user_authentication(client_entry* client , const char* username);

//This will create a new client entry for the new user.
client_entry* allocate_resource_new_client(pid_t current_client_pid);
client_entry* create_connection(pid_t current_client_pid);

//This function is responsible for tackling the message
int handle_one_client_command(client_entry* client , char* line);

//This function is responsible for reading a complete line of command(this may include several commands)
int handle_client_message(client_entry* client);

//When the client sends a login message to the server, this action will be treated to send a message
//to the signal pipe, and then I can use poll to monitor the new connection
void accept_clients_via_pipes(void);

//The first monitored fd is the signal pipe fd
nfds_t update_poll(void);

//This is used to check whether it's up to one second when promising to disconnect the client
int time_over_check(const struct timespec disconnect_time);
void delete_timeout_clients(void);
void close_signal_pipes(void);
int server_event_poll(void);
void delete_all_service_resources(void);





#endif