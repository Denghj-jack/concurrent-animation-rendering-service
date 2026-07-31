#ifndef SERVER_RESOURCES_H
#define SERVER_RESOURCES_H

#include <stdint.h>
#include <pthread.h>

#include "animate_tables.h"
#include "server_struct_types.h"

extern canvas_entry* canvas_head;
extern sprite_entry* sprite_head;
extern placement_entry* placement_head;

extern pthread_mutex_t canvas_lock;
extern pthread_mutex_t sprite_lock;
extern pthread_mutex_t placement_lock;


//Each sprite entry has its own id, which help prevent from memory address reveal
sprite_entry* sprite_entry_search_via_id(uint64_t id); 
placement_entry* placement_entry_search_via_id(uint64_t id);
canvas_entry* canvas_entry_search_via_id(uint64_t id);


int canvas_shared_access_check(canvas_entry* canvas, char* username);
int user_access_canvas_check(canvas_entry* canvas, char* username);
int user_access_sprite_check(sprite_entry* sprite, char* username);
int user_access_placement_check(placement_entry* placement, char* username);


sprite_entry* create_sprite(void* sprite_pointer , char* owner_username);
placement_entry* create_placement(void* placement_pointer , char* owner_username , uint64_t canvas_id , uint64_t sprite_id);
canvas_entry* create_canvas(void* canvas_pointer , char* owner_username , size_t height , size_t width);

void free_shared_users(shared_user_entry* shared_user_head);
void remove_canvas_inactive(uint64_t id);
void remove_sprite_inactive(uint64_t id);
void remove_placement_inactive(uint64_t id);

int add_shared_user_to_userlist(canvas_entry* canvas , char* username);

void delete_placements_in_inactive_canvas(uint64_t canvas_id);
void delete_all_animate_resources(void);


#endif