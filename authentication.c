#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "authentication.h"
#include "configuration.h"

int check_username(const char* username){
    if(username == NULL){
        return 1;
    }
    size_t username_length = strlen(username);
    //Just in case someone wants to break the program when typing long name
    if(username_length == 0 || username_length > USERNAME_MAX_SIZE){ 
        return 1;
    } 
    return 0;
}

int search_user_info(const char* username , long* balance_found){//-1:failed to open file 1:username found 0:username not found
    FILE* file = fopen("users.txt" , "r");
    if(file == NULL){
        perror("Failed to open the users.txt file");
        return -1;
    }
    char line[INPUT_MAX_SIZE];
    while(fgets(line , sizeof(line) , file) != NULL){
        char* last_point = NULL;
        char* token_username = strtok_r(line, " \t\r\n" , &last_point);
        if (token_username == NULL){//This indicates this is blank line
            continue;
        }
        char* token_balance = strtok_r(NULL , " \t\r\n" , &last_point);
        if(token_balance == NULL){
            continue;//missing balance
        }
        if(strcmp(token_username , username) != 0){
            continue;
        }
        long balance = atoi(token_balance);
        *balance_found = balance;
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

int username_exists(char* username){
    if(check_username(username) == 1){
        return 0;
    }
    long balance = 0;
    if(search_user_info(username , &balance) != 1){
        return 0;
    }
    return 1;
}