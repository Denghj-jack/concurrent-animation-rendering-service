#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "server_resources.h"
#include "configuration.h"
#include "libanimate/include/animate/animate.h"

//These few pointers are served as the head of linked list for corresponding entry
canvas_entry* canvas_head = NULL;
sprite_entry* sprite_head = NULL;
placement_entry* placement_head = NULL;

static uint64_t canvas_id_counter = 1; // id -> 0 is considered as invalid id
static uint64_t sprite_id_counter = 1;// id -> 0 is considered as invalid id
static uint64_t placement_id_counter = 1; // id -> 0 is considered as invalid id

pthread_mutex_t canvas_lock = PTHREAD_MUTEX_INITIALIZER;//protect the canvas linked list
pthread_mutex_t sprite_lock = PTHREAD_MUTEX_INITIALIZER;//protect the sprite linked list
pthread_mutex_t placement_lock = PTHREAD_MUTEX_INITIALIZER;//protect the placement linked list


sprite_entry* sprite_entry_search_via_id(uint64_t id){
    sprite_entry* current = sprite_head;
    if(id == 0){
        return NULL;
    }
    while(current != NULL){
        if(current -> id == id && current -> active_status == 1){
            return current;
        }
        current = current -> next;
    }
    return NULL;
}

placement_entry* placement_entry_search_via_id(uint64_t id){
    placement_entry* current = placement_head;
    if(id == 0){
        return NULL;
    }
    while(current != NULL){
        if(current -> id == id && current -> active_status == 1){
            return current;
        }
        current = current -> next;
    }
    return NULL;
}

canvas_entry* canvas_entry_search_via_id(uint64_t id){
    canvas_entry* current = canvas_head;
    if(id == 0){
        return NULL;
    }
    while(current != NULL){
        if(current -> id == id && current -> active_status == 1){
            return current;
        }
        current = current -> next;
    }
    return NULL;
}

//This function is used to check whether this user is in the shared user list of the given canvas
//return 1-> have access 
//retrun 0 -> no access
int canvas_shared_access_check(canvas_entry* canvas , char* username){
    if(canvas == NULL || username == NULL){
        return 0;
    }
    shared_user_entry* current = canvas -> shared_user_list;
    while(current != NULL){
        if(strcmp(current ->username , username) == 0){
            return 1;//returning 1 represents this user has access to the canvas
        }
        current = current ->next;
    }
    return 0;
}

//This function is used to check whether this user can access this canvas, including the owner of the canvas
//return 1-> have access 
//retrun 0 -> no access
int user_access_canvas_check(canvas_entry* canvas , char* username){
    if(canvas == NULL || username == NULL){
        return 0; //invalid input 
    }
    if(canvas -> active_status == 0){
        return 0;//The canvas has been deleted
    }
    if(strcmp(canvas ->owner_username , username) == 0){
        return 1;
    }
    if(canvas_shared_access_check(canvas , username) == 1){
        return 1;
    }
    return 0;
}

int user_access_sprite_check(sprite_entry* sprite , char* username){
    if(sprite == NULL || username == NULL || sprite ->active_status == 0){
        return 0;
    }
    if(strcmp(sprite ->owner_username , username) == 0){
        return 1;
    }
    else{
        return 0;
    }
}

//This will check whether the user can access the canvas where this placement exists
int user_access_placement_check(placement_entry* placement , char* username){
    if(placement == NULL || username == NULL || placement -> active_status == 0){
        return 0;
    }
    if(strcmp(placement -> owner_username , username) == 0){
        return 1;
    }
    return 0;
    
}

//This will create the sprite and inset it into the sprite linked list
sprite_entry* create_sprite(void* sprite_pointer , char* owner_username){
    if(sprite_pointer == NULL || owner_username == NULL){
        return NULL;
    }
    sprite_entry* entry = (sprite_entry*)calloc(1 , sizeof(sprite_entry));
    if(entry == NULL){
        return NULL;
    }
    
    entry->sprite_pointer = sprite_pointer;
    entry ->active_status = 1;
    strncpy(entry -> owner_username , owner_username , USERNAME_MAX_SIZE);
    entry -> owner_username[USERNAME_MAX_SIZE] = '\0'; //This prevents from large username danger
    entry ->next = sprite_head;
    sprite_head = entry;
    entry -> id = sprite_id_counter;
    sprite_id_counter++;
    return entry;
}


placement_entry* create_placement(void* placement_pointer , char* owner_username , uint64_t canvas_id , uint64_t sprite_id){
    if(placement_pointer == NULL || owner_username == NULL || canvas_id <= 0 || sprite_id <= 0){
        return NULL;
    }
    placement_entry* entry = (placement_entry*)calloc(1 , sizeof(placement_entry));
    if(entry == NULL){
        return NULL;
    }
    entry -> active_status = 1;
    entry ->placement_pointer = placement_pointer;
    strncpy(entry -> owner_username , owner_username , USERNAME_MAX_SIZE);
    entry -> owner_username[USERNAME_MAX_SIZE] = '\0';
    entry ->canvas_id = canvas_id;
    entry -> sprite_id = sprite_id;

    entry-> id = placement_id_counter;
    placement_id_counter++;
    entry -> next = placement_head;
    placement_head = entry;
    return entry;

}

canvas_entry* create_canvas(void* canvas_pointer , char* owner_username , size_t height , size_t width){
    if(canvas_pointer == NULL || owner_username == NULL){
        return NULL;
    }
    canvas_entry* entry = (canvas_entry*)calloc(1 , sizeof(canvas_entry));
     if(entry == NULL){
        return NULL;
    }
    entry -> height = height;
    entry -> width = width;
    entry-> active_status = 1;
    entry ->canvas_pointer = canvas_pointer;
    strncpy(entry -> owner_username , owner_username , USERNAME_MAX_SIZE);
    entry -> owner_username[USERNAME_MAX_SIZE] = '\0';
    entry->id = canvas_id_counter;
    canvas_id_counter++;
    entry-> next = canvas_head;
    canvas_head = entry;
    entry->shared_user_list = NULL;
    entry -> barrier_count = 0;
    entry ->barrier_round_num = 0;
    if(pthread_cond_init(&entry ->barrier_cond , NULL) != 0){
        free(entry);
        entry = NULL;
        return NULL;
    }
    //This read-write lock is used for parallel-operations
    if(pthread_rwlock_init(&entry ->rwlock , NULL) != 0){
        pthread_cond_destroy(&entry -> barrier_cond);
        free(entry);
        return NULL;
    }
    return entry;
}


void free_shared_users(shared_user_entry* shared_user_head){
    shared_user_entry* current = shared_user_head;
    shared_user_entry* next = NULL;
    while(current != NULL){
        next = current ->next;
        free(current);
        current = next;
    }
}

void remove_canvas_inactive(uint64_t id){
    canvas_entry* current = canvas_head;
    canvas_entry* previous = NULL;
    while(current != NULL){
        if(current ->id == id){
            if(previous == NULL){//This indicates that the current canvas is the head of the canvas linked list
                canvas_head = current -> next;
            }
            else{
                previous -> next = current -> next;
            }

            pthread_cond_destroy(&current ->barrier_cond);
            pthread_rwlock_destroy(&current -> rwlock);

            free_shared_users(current -> shared_user_list);
            free(current);
            return;
        }
        previous = current;
        current = current-> next;
    }
}

void remove_sprite_inactive(uint64_t id){
    sprite_entry* current = sprite_head;
    sprite_entry* previous = NULL;
    while(current != NULL){
        if(current ->id == id){
            if(previous == NULL){//This indicates that the current sprite is the head of the sprite linked list
                sprite_head = current -> next;
            }
            else{
                previous -> next = current -> next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current-> next;
    }
}



void remove_placement_inactive(uint64_t id){
    placement_entry* current = placement_head;
    placement_entry* previous = NULL;
    while(current != NULL){
        if(current ->id == id){
            if(previous == NULL){//This indicates that the current placement is the head of the placement linked list
                placement_head = current -> next;
            }
            else{
                previous -> next = current -> next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current-> next;
    }
    return;
}


//0 -> successful to add the user to the share-user list
// -1 -> parameter or memory allocation error 
int add_shared_user_to_userlist(canvas_entry* canvas , char* username){
    if(canvas == NULL || username == NULL){
        return -1;
    }
    if(canvas_shared_access_check(canvas , username) == 1){//This user is already in the shared-user list
        return 0;
    }
    shared_user_entry* node = (shared_user_entry*)calloc(1 , sizeof(shared_user_entry));
    if(node == NULL){
        return -1;
    }
    node ->next = canvas->shared_user_list;
    canvas -> shared_user_list = node;
    strncpy(node->username , username , USERNAME_MAX_SIZE);
    node->username[USERNAME_MAX_SIZE] = '\0';
    return 0;
}

void delete_placements_in_inactive_canvas(uint64_t canvas_id){
    placement_entry* current = placement_head;
    placement_entry* previous = NULL;
    placement_entry* next = NULL;
    while(current != NULL){
        next = current -> next;
        if(current ->canvas_id == canvas_id){
            if(previous == NULL){//This indicates that the current placement is the head of the placement linked list
                placement_head = next;
            }
            else{
                previous -> next = next;
            }
            free(current);
        }
        else{
            previous = current;
        }
        current = next;
    }
    return;
}

void delete_all_animate_resources(void){
    canvas_entry* entry = canvas_head;
    canvas_entry* canvas_next = NULL;
    while(entry != NULL){
        canvas_next = entry -> next;
        if(entry ->active_status == 1 && entry -> canvas_pointer != NULL){
            animate_destroy_canvas(entry -> canvas_pointer);
        }
        pthread_cond_destroy(&entry -> barrier_cond);
        pthread_rwlock_destroy(&entry -> rwlock);
        free_shared_users(entry -> shared_user_list);
        free(entry);
        entry = canvas_next;
    }
    canvas_head = NULL;



    placement_entry* placement = placement_head;
    placement_entry* placement_next = NULL;
    while(placement != NULL){
        placement_next = placement -> next;
        free(placement);
        placement = placement_next;
    }
    placement_head = NULL;



    sprite_entry* sprite = sprite_head;
    sprite_entry* sprite_next = NULL;
    while(sprite != NULL){
        sprite_next = sprite ->next;
        if(sprite -> active_status == 1 && sprite -> sprite_pointer != NULL){
            animate_destroy_sprite(sprite -> sprite_pointer);
        }
        free(sprite);
        sprite = sprite_next;
    }
    sprite_head = NULL;
}

