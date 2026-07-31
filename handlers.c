#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include "libanimate/include/animate/animate.h"
#include "rpc_preprocess.h"
#include "server_struct_types.h"
#include "server_resources.h"
#include "server_connection.h"
#include "authentication.h"


static int concatenate_output_filename(char* output_filename , size_t output_size , char* input_filename , char* extension){
    if(output_filename == NULL || input_filename == NULL || extension == NULL){
        return -1;
    }
    snprintf(output_filename , output_size , "%s%s" , input_filename , extension);
    return 0;
}



static int run_ffmpeg(char* dat_file , char* mp4_file , char* log_file , size_t height , size_t width , size_t frame_rate){
    if(dat_file == NULL || mp4_file == NULL || log_file == NULL){
        return -1;
    }

    char height_and_width_str[64];
    char frame_rate_str[64];
    snprintf(height_and_width_str , sizeof(height_and_width_str) , "%zux%zu" , height , width);
    snprintf(frame_rate_str , sizeof(frame_rate_str) , "%zu" , frame_rate);

    int log_fd = open(log_file , O_WRONLY | O_CREAT | O_TRUNC , 0666);
    if(log_fd == -1){
        return -1;
    }
    pid_t pid = fork();
    if(pid == -1){
        close(log_fd);
        return -1;
    }

    if(pid == 0){
        dup2(log_fd , STDOUT_FILENO);
        dup2(log_fd , STDERR_FILENO);
        close(log_fd);

        execlp("ffmpeg" , "ffmpeg" , "-f" , "rawvideo" , "-pix_fmt" , "bgra" , "-s" , height_and_width_str , "-r" , frame_rate_str , "-i" , dat_file , "-c:v" , "libx264" , "-y" , mp4_file , NULL);
        _exit(127); //usually this step will not run, as the current child process has been replaced
    }//_exit() is better than normal exit
    else{
        close(log_fd);
        int status;
        if(waitpid(pid , &status , 0) == -1){
            return -1;
        }
        
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0){
            return 0;
        }
        return -1;
    }
    return -1;
}


int handle_generate(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    
    char dat_file[128];
    char mp4_file[128];
    char log_file[128];
    uint64_t canvas_id;
    size_t frame_rate;
    size_t start;
    size_t end;

    if(num_tokens_check(commands -> token_count , 6) == 0){
        return create_rpc_error_message(response , response_size);
    }
    char* filename = commands -> tokens[2];
    if(convert_uint64_parameter(commands -> tokens[1] , &canvas_id) == -1 || 
    convert_size_t_token(commands-> tokens[3] , &start) == -1 ||
    convert_size_t_token(commands-> tokens[4] , &end) == -1 ||
    convert_size_t_token(commands -> tokens[5] , &frame_rate) == -1){
        return create_value_error_message(response , response_size);
    }
    if (start > end){
        return create_value_error_message(response , response_size);
    }

    if(concatenate_output_filename(dat_file , sizeof(dat_file) , filename , ".dat") == -1 ||
    concatenate_output_filename(mp4_file , sizeof(mp4_file) , filename , ".mp4") == -1 ||
    concatenate_output_filename(log_file , sizeof(log_file) , filename , ".log") == -1){
        return create_value_error_message(response , response_size);
    }



    pthread_mutex_lock(&canvas_lock);
    canvas_entry* canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL || user_access_canvas_check(canvas , client-> username ) == 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response, response_size);
    }


    if(pthread_rwlock_rdlock(&canvas -> rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }
    struct canvas* canvas_pointer = (struct canvas*)canvas -> canvas_pointer;
    size_t height = canvas -> height;
    size_t width = canvas -> width;

    pthread_mutex_unlock(&canvas_lock);

    size_t frame_size = animate_frame_size_bytes(canvas -> canvas_pointer);
    color_t* frame_buffer = (color_t*)calloc(1 , frame_size);

    if(frame_buffer == NULL){
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_internal_error_message(response , response_size);
    }

    FILE* dat_file_pointer = fopen(dat_file , "wb");
    if(dat_file_pointer == NULL){
        free(frame_buffer);
        frame_buffer = NULL;
        pthread_rwlock_unlock(&canvas -> rwlock);
        snprintf(response , response_size , "0 -1\n");
        return 0;
    }

    for(size_t num = start ; num <= end ; num++){
        animate_generate_frame(canvas_pointer , num , frame_rate , frame_buffer);
        if(fwrite(frame_buffer , 1 , frame_size , dat_file_pointer) != frame_size){
            fclose(dat_file_pointer);
            free(frame_buffer);
            pthread_rwlock_unlock(&canvas -> rwlock);
            snprintf(response , response_size , "0 -1\n");
            return 0;

        }
    }
    fclose(dat_file_pointer);
    free(frame_buffer);
    pthread_rwlock_unlock(&canvas -> rwlock);
    if(run_ffmpeg(dat_file , mp4_file , log_file , height , width , frame_rate) == -1){
        snprintf(response , response_size , "0 0 -1\n");
        return 0;
    }
    snprintf(response , response_size , "0 0 0\n");
    return 0;
}



int handle_shared_canvas(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    uint64_t canvas_id;
    canvas_entry* canvas;
    if(num_tokens_check(commands->token_count , 3) == 0){
        return create_rpc_error_message(response , response_size);
    }

    if(convert_uint64_parameter(commands -> tokens[1] , &canvas_id) == -1){
        return create_value_error_message(response , response_size);
    }

    char* adding_username = commands -> tokens[2];
    if(username_exists(adding_username) == 0){
        return create_value_error_message(response , response_size);
    }
    

    pthread_mutex_lock(&canvas_lock);
    canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL || user_access_canvas_check(canvas , client -> username) == 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }
    if(add_shared_user_to_userlist(canvas , adding_username) == -1){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_unlock(&canvas_lock);
    return create_success_message(response , response_size);
}



int handle_barrier(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    if(num_tokens_check(commands -> token_count  , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    uint64_t canvas_id;
    if(convert_uint64_parameter(commands -> tokens[1] , &canvas_id)== -1){
        return create_value_error_message(response , response_size);
    }

    pthread_mutex_lock(&canvas_lock);
    canvas_entry* canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL || user_access_canvas_check(canvas , client -> username) == 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }

    canvas ->barrier_count++;

    int current_barrier_round = canvas ->barrier_round_num;


    while(1){
    pthread_mutex_lock(&client_head_lock);
    int connected_users_num = count_connected_canvas_users(canvas);
    if(connected_users_num == -1){
        pthread_mutex_unlock(&client_head_lock);
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }
    pthread_mutex_unlock(&client_head_lock);

    if(canvas -> barrier_count >= connected_users_num){//This will also count for where some user disconnects
        canvas-> barrier_round_num++;
        canvas ->barrier_count = 0;
        pthread_cond_broadcast(&canvas -> barrier_cond);
        pthread_mutex_unlock(&canvas_lock);
        return create_success_message(response , response_size);
    }

    if(canvas -> barrier_round_num > current_barrier_round){
        pthread_mutex_unlock(&canvas_lock);
        return create_success_message(response , response_size);
    }
    if(canvas ->active_status == 0 || client -> closing_status == 1){
        if(canvas -> barrier_count > 0){
            canvas -> barrier_count--;
        }
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }
    pthread_cond_wait(&canvas-> barrier_cond , &canvas_lock);
    //This allows the worker thread to do other tasks when not enough users send the barrier command
    }
}



int handle_set_animation_params(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    uint64_t sprite_placement_id;
    placement_entry* placement;
    ssize_t vx;
    ssize_t vy;
    ssize_t ax;
    ssize_t ay;
    if(num_tokens_check(commands->token_count , 6) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands -> tokens[1] , &sprite_placement_id) == -1  || 
    convert_ssize_t_token(commands -> tokens[2] , &vx) == -1 ||
    convert_ssize_t_token(commands -> tokens[3] , &vy) == -1 ||
    convert_ssize_t_token(commands -> tokens[4] , &ax) == -1 ||
    convert_ssize_t_token(commands -> tokens[5] , &ay) == -1){
        return create_value_error_message(response , response_size);
    }


    pthread_mutex_lock(&placement_lock);
    placement = placement_entry_search_via_id(sprite_placement_id);
    if(placement == NULL){
        pthread_mutex_unlock(&placement_lock);
        return create_value_error_message(response , response_size);
    }

    uint64_t canvas_id = placement->canvas_id;
    pthread_mutex_unlock(&placement_lock);

    pthread_mutex_lock(&canvas_lock);
    canvas_entry* canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }
    //Every time when I want to modify anything realted to the canvas resources
    //I need to use write lock to lock the canvas
    if(pthread_rwlock_wrlock(&canvas -> rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }
    pthread_mutex_unlock(&canvas_lock);

    pthread_mutex_lock(&placement_lock);
    placement = placement_entry_search_via_id(sprite_placement_id);
    if(placement == NULL || placement -> canvas_id != canvas_id){
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response , response_size);
    }

    if(user_access_placement_check(placement , client -> username) == 0){
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response , response_size);
    }
    animate_set_animation_params(placement->placement_pointer , vx , vy , ax , ay);

    pthread_mutex_unlock(&placement_lock);
    pthread_rwlock_unlock(&canvas -> rwlock);
    return create_success_message(response , response_size);
}



int handle_create_sprite(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    char* file_path;
    struct sprite* sprite_pointer;
    sprite_entry* entry;
    if(num_tokens_check(commands->token_count , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    file_path = commands ->tokens[1];
    sprite_pointer = animate_create_sprite(file_path);
    if(sprite_pointer == NULL){
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_lock(&sprite_lock);
    entry = create_sprite(sprite_pointer , client->username);
    pthread_mutex_unlock(&sprite_lock);

    if(entry == NULL){
        animate_destroy_sprite(sprite_pointer);
        return create_internal_error_message(response , response_size);
    }
    return create_success_uint64_message(response , response_size , entry ->id);
}



int handle_create_rectangle(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    size_t width;
    size_t height;
    int filled;
    color_t color;
    struct sprite* sprite_pointer;
    sprite_entry* entry;
    if(num_tokens_check(commands->token_count , 5) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_size_t_token(commands -> tokens[1] , &width) == -1  || 
    convert_size_t_token(commands -> tokens[2] , &height) == -1 ||
    convert_color_t_token(commands -> tokens[3] , &color) == -1 ||
    convert_bool_token(commands -> tokens[4] , &filled) == -1){
        return create_value_error_message(response , response_size);
    }
    sprite_pointer = animate_create_rectangle(width , height , color , (bool)filled);
    if(sprite_pointer == NULL){
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_lock(&sprite_lock);
    entry = create_sprite(sprite_pointer , client->username);
    pthread_mutex_unlock(&sprite_lock);

    if(entry == NULL){
        animate_destroy_sprite(sprite_pointer);
        return create_internal_error_message(response , response_size);
    }
    return create_success_uint64_message(response , response_size , entry ->id);
}

int handle_create_circle(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    size_t radius;
    int filled;
    color_t color;
    struct sprite* sprite_pointer;
    sprite_entry* entry;
    if(num_tokens_check(commands->token_count , 4) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_size_t_token(commands -> tokens[1] , &radius) == -1  || 
    convert_color_t_token(commands -> tokens[2] , &color) == -1 ||
    convert_bool_token(commands -> tokens[3] , &filled) == -1){
        return create_value_error_message(response , response_size);
    }
    sprite_pointer = animate_create_circle(radius , color , (bool)filled);
    if(sprite_pointer == NULL){
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_lock(&sprite_lock);
    entry = create_sprite(sprite_pointer , client->username);
    pthread_mutex_unlock(&sprite_lock);

    if(entry == NULL){
        animate_destroy_sprite(sprite_pointer);
        return create_internal_error_message(response , response_size);
    }
    return create_success_uint64_message(response , response_size , entry ->id);
}


int handle_place_sprite(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    ssize_t x;
    ssize_t y;
    uint64_t canvas_id;
    uint64_t sprite_id;
    canvas_entry* canvas_entry;
    sprite_entry* sprite_entry;
    struct sprite_placement* placement_pointer;
    placement_entry* placement_entry;
    if(num_tokens_check(commands->token_count , 5) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands -> tokens[1] , &canvas_id) == -1  || 
    convert_uint64_parameter(commands -> tokens[2] , &sprite_id) == -1 ||
    convert_ssize_t_token(commands -> tokens[3] , &x) == -1 ||
    convert_ssize_t_token(commands-> tokens[4] , &y) == -1){
        return create_value_error_message(response , response_size);
    }

    pthread_mutex_lock(&canvas_lock);
    canvas_entry = canvas_entry_search_via_id(canvas_id);
    if(canvas_entry == NULL || user_access_canvas_check(canvas_entry , client -> username) == 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }

    if(pthread_rwlock_wrlock(&canvas_entry ->rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response, response_size);
    }
    pthread_mutex_unlock(&canvas_lock);



    pthread_mutex_lock(&sprite_lock);
    sprite_entry = sprite_entry_search_via_id(sprite_id);
    if(sprite_entry == NULL || user_access_sprite_check(sprite_entry , client->username) == 0){
        pthread_mutex_unlock(&sprite_lock);
        pthread_rwlock_unlock(&canvas_entry -> rwlock);
        return create_value_error_message(response , response_size);
    }

    placement_pointer = animate_place_sprite((struct canvas*)canvas_entry ->canvas_pointer , (struct sprite*)sprite_entry->sprite_pointer , x, y);

    pthread_mutex_unlock(&sprite_lock);

    if(placement_pointer == NULL){
        pthread_rwlock_unlock(&canvas_entry -> rwlock);
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_lock(&placement_lock);
    placement_entry = create_placement(placement_pointer , client->username , canvas_id , sprite_id);

    pthread_mutex_unlock(&placement_lock);


    if(placement_entry == NULL){
        animate_destroy_placement(placement_pointer);
        pthread_rwlock_unlock(&canvas_entry -> rwlock);
        return create_internal_error_message(response , response_size);
    }

    pthread_rwlock_unlock(&canvas_entry -> rwlock);
    return create_success_uint64_message(response , response_size , placement_entry ->id);

}



int handle_placement_operation(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    uint64_t placement_id;
    placement_entry* placement;
    if(num_tokens_check(commands->token_count , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands -> tokens[1] , &placement_id) == -1){
        return create_value_error_message(response , response_size);
    }

    pthread_mutex_lock(&placement_lock);
    placement = placement_entry_search_via_id(placement_id);
    if(placement == NULL){
        pthread_mutex_unlock(&placement_lock);
        return create_value_error_message(response , response_size);
    }
    uint64_t canvas_id = placement -> canvas_id;
    pthread_mutex_unlock(&placement_lock);



    pthread_mutex_lock(&canvas_lock);

    canvas_entry* canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }
    if(pthread_rwlock_wrlock(&canvas -> rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_unlock(&canvas_lock);

    pthread_mutex_lock(&placement_lock);

    placement = placement_entry_search_via_id(placement_id);

    if(placement == NULL || placement->canvas_id != canvas_id){
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response, response_size);
    }

    if(user_access_placement_check(placement, client->username) == 0){
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response, response_size);
    }

    
    if(strcmp(commands -> tokens[0]  , "placement_up")== 0){
        animate_placement_up((struct sprite_placement*) placement ->placement_pointer);
    }
    else if(strcmp(commands -> tokens[0]  , "placement_down")== 0){
        animate_placement_down((struct sprite_placement*) placement ->placement_pointer);
    }
    else if(strcmp(commands -> tokens[0]  , "placement_top")== 0){
        animate_placement_top((struct sprite_placement*) placement ->placement_pointer);
    }
    else if(strcmp(commands -> tokens[0]  , "placement_bottom")== 0){
        animate_placement_bottom((struct sprite_placement*) placement ->placement_pointer);
    }
    else{
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_rpc_error_message(response , response_size);
    }

    pthread_mutex_unlock(&placement_lock);
    pthread_rwlock_unlock(&canvas -> rwlock);

    return create_success_message(response , response_size);
}




int handle_create_canvas(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    size_t height;
    size_t width;
    color_t background_color;
    canvas_entry* entry;
    struct canvas* canvas_pointer;
    if(num_tokens_check(commands->token_count , 4) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_size_t_token(commands -> tokens[1] , &height) == -1  || 
    convert_size_t_token(commands -> tokens[2] , &width) == -1 ||
    convert_color_t_token(commands -> tokens[3] , &background_color) == -1){
        return create_value_error_message(response , response_size);
    }
    canvas_pointer = animate_create_canvas(height , width , background_color);
    if(canvas_pointer == NULL){
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_lock(&canvas_lock);
    entry = create_canvas(canvas_pointer , client ->username , height , width);
    pthread_mutex_unlock(&canvas_lock);

    if(entry == NULL){
        animate_destroy_canvas(canvas_pointer);
        return create_internal_error_message(response , response_size);
    }
    return create_success_uint64_message(response , response_size , entry ->id);
}


int handle_destroy_placement(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    uint64_t placement_id;
    placement_entry* placement;
    if(num_tokens_check(commands->token_count , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands ->tokens[1] , &placement_id) == -1){
        return create_value_error_message(response , response_size);
    }


    pthread_mutex_lock(&placement_lock);
    placement = placement_entry_search_via_id(placement_id);
    if(placement == NULL){
        pthread_mutex_unlock(&placement_lock);
        return create_value_error_message(response , response_size);
    }

    uint64_t canvas_id = placement->canvas_id;
    pthread_mutex_unlock(&placement_lock);



    pthread_mutex_lock(&canvas_lock);

    canvas_entry* canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }
    if(pthread_rwlock_wrlock(&canvas -> rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }

    pthread_mutex_unlock(&canvas_lock);
    pthread_mutex_lock(&placement_lock);

    placement = placement_entry_search_via_id(placement_id);
    if(placement == NULL || placement->canvas_id != canvas_id){//re-check This will check whether someone destroyed the canvas just now 
        
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response, response_size);
    }

    if(user_access_placement_check(placement, client->username) == 0){
        pthread_mutex_unlock(&placement_lock);
        pthread_rwlock_unlock(&canvas -> rwlock);
        return create_value_error_message(response, response_size);
    }

    animate_destroy_placement(placement ->placement_pointer);
    remove_placement_inactive(placement_id);

    pthread_mutex_unlock(&placement_lock);
    pthread_rwlock_unlock(&canvas -> rwlock);



    return create_success_message(response , response_size);
}



int handle_destroy_sprite(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    uint64_t sprite_id;
    sprite_entry* sprite;
    
    if(num_tokens_check(commands->token_count , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands ->tokens[1] , &sprite_id) == -1){
        return create_value_error_message(response , response_size);
    }

    pthread_mutex_lock(&sprite_lock);
    sprite = sprite_entry_search_via_id(sprite_id);
    
    if(sprite == NULL){
        pthread_mutex_unlock(&sprite_lock);
        return create_value_error_message(response , response_size);
    }
    if(user_access_sprite_check(sprite , client -> username) == 0){
        pthread_mutex_unlock(&sprite_lock);
        return create_value_error_message(response , response_size);
    }


    bool result = animate_destroy_sprite(sprite ->sprite_pointer);
    if(result == 0){
         remove_sprite_inactive(sprite_id);
    }

    pthread_mutex_unlock(&sprite_lock);
    return create_success_uint64_message(response , response_size , (uint64_t)result);
}




int handle_destroy_canvas(client_entry* client , rpc_commands* commands, char* response , size_t response_size){
    uint64_t canvas_id;
    canvas_entry* canvas;
    if(num_tokens_check(commands->token_count , 2) == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(convert_uint64_parameter(commands ->tokens[1] , &canvas_id) == -1){
        return create_value_error_message(response , response_size);
    }


    pthread_mutex_lock(&canvas_lock);
    canvas = canvas_entry_search_via_id(canvas_id);
    if(canvas == NULL){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }
    if(user_access_canvas_check(canvas , client -> username) == 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }

    if(canvas -> barrier_count > 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }

    pthread_mutex_lock(&client_head_lock);
    int connected_users_num = count_connected_canvas_users(canvas);
    pthread_mutex_unlock(&client_head_lock);

    if(connected_users_num == -1){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }

    if(connected_users_num > 1){
        pthread_mutex_unlock(&canvas_lock);
        return create_value_error_message(response , response_size);
    }

    if(pthread_rwlock_wrlock(&canvas -> rwlock) != 0){
        pthread_mutex_unlock(&canvas_lock);
        return create_internal_error_message(response , response_size);
    }
    canvas -> active_status =0;
    pthread_mutex_unlock(&canvas_lock);
    pthread_mutex_lock(&placement_lock);

    animate_destroy_canvas(canvas ->canvas_pointer);
    delete_placements_in_inactive_canvas(canvas_id);
    pthread_mutex_unlock(&placement_lock);

    pthread_rwlock_unlock(&canvas -> rwlock);
    pthread_mutex_lock(&canvas_lock);
    
    remove_canvas_inactive(canvas_id);

    pthread_mutex_unlock(&canvas_lock);

    return create_success_message(response , response_size);
}