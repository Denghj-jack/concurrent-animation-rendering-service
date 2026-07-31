#include "server_dispatch_work.h"
#include <string.h>
#include "handlers.h"
#include "rpc_preprocess.h"


void run_rpc_command(client_entry* client , char* line , char* response , size_t response_size){
    rpc_commands commands;
    memset(&commands , 0 , sizeof(commands));
    if(split_rpc_commands(line , &commands) == -1){
        create_rpc_error_message(response , response_size);
        return;
    }
    process_rpc_command(client , &commands , response , response_size);
}

int process_rpc_command(client_entry* client , rpc_commands* commands , char* response , size_t response_size){
    if(client == NULL || commands == NULL || commands->token_count == 0){
        return create_rpc_error_message(response , response_size);
    }
    if(strcmp(commands->tokens[0] , "create_sprite") == 0){
        return handle_create_sprite(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "create_rectangle") == 0){
        return handle_create_rectangle(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "create_circle") == 0){
        return handle_create_circle(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "destroy_sprite") == 0){
        return handle_destroy_sprite(client , commands, response , response_size);
    }
    //This includes handling all movement operations of placement(up , down , top , bottom)
    if(strcmp(commands->tokens[0] , "place_sprite") == 0){
        return handle_place_sprite(client , commands, response , response_size);
    }

    //The above four operations use the same handlers
    if(strcmp(commands->tokens[0] , "placement_up") == 0){
        return handle_placement_operation(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "placement_down") == 0){
        return handle_placement_operation(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "placement_top") == 0){
        return handle_placement_operation(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "placement_bottom") == 0){
        return handle_placement_operation(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "destroy_placement") == 0){
        return handle_destroy_placement(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "create_canvas") == 0){
        return handle_create_canvas(client , commands, response , response_size);
    }
    if(strcmp(commands->tokens[0] , "destroy_canvas") == 0){
        return handle_destroy_canvas(client , commands, response , response_size);
    }
    if(strcmp(commands -> tokens[0] , "set_animation_params") == 0){
        return handle_set_animation_params(client , commands, response , response_size);
    }
    if(strcmp(commands -> tokens[0] , "generate") == 0){
        return handle_generate(client , commands, response , response_size);
    }
    if(strcmp(commands -> tokens[0] , "share_canvas") == 0){
        return handle_shared_canvas(client , commands, response , response_size);
    }
    if(strcmp(commands -> tokens[0] , "barrier") == 0){
        return handle_barrier(client , commands, response , response_size);
    }
    return create_rpc_error_message(response , response_size);
} 