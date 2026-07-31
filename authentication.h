#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

int check_username(const char* username);


//This function will search in the users.txt file to see whether
//the given username is listed, and check the balance
//-1:failed to open file 1:username found 0:username not found
int search_user_info(const char* username , long* balance_found);

int username_exists(char* username);


#endif