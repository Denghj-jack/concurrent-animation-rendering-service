#ifndef ANIMATE_TABLES_H
#define ANIMATE_TABLES_H


#include <stdint.h>
#include <pthread.h>
#include <stddef.h>
#include "libanimate/include/animate/animate.h"
#include "configuration.h"



typedef struct sprite_entry{
    uint64_t id;
    char owner_username[USERNAME_MAX_SIZE+1]; //Adding one is prevent from max length(32) of usernmae
    int active_status;
    struct sprite_entry* next;
    struct sprite* sprite_pointer;
}sprite_entry;


typedef struct placement_entry{
    uint64_t id;
    char owner_username[USERNAME_MAX_SIZE+1];//Adding one is prevent from max length(32) of usernmae
    int active_status;
    uint64_t canvas_id;
    uint64_t sprite_id;
    struct placement_entry* next;
    struct sprite_placement* placement_pointer;
}placement_entry;

typedef struct shared_user_entry{
    char username[USERNAME_MAX_SIZE+1];
    struct shared_user_entry* next;
}shared_user_entry;


typedef struct canvas_entry{
    uint64_t id;
    char owner_username[USERNAME_MAX_SIZE+1]; //Adding one is prevent from max length(32) of usernmae
    int active_status;
    struct canvas_entry* next; //A linked list
    struct canvas* canvas_pointer;
    shared_user_entry* shared_user_list; //This is the head of linked list of shared user of this canvas
    size_t height;
    size_t width;
    int barrier_count; //num of users who has sent harrier
    int barrier_round_num;
    pthread_cond_t barrier_cond; 
    pthread_rwlock_t rwlock;//used for parallel generation
}canvas_entry;



#endif