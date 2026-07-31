#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "configuration.h"
#include "client_connection.h"


static volatile sig_atomic_t sigusr2_flag = 0;
static volatile sig_atomic_t timeout_flag = 0;



static void sigusr2_handler(int signum){
    (void)signum;
    sigusr2_flag = 1; //when receiving SIGUSR2, set the flag to 1
    return;
}


static void sig_timeout_handler(int signum){
    (void)signum;
    timeout_flag = 1; 
    return;
}


int connection_to_server(pid_t server_pid , int* c2s_fd , int* s2c_fd){
    sigset_t block_mask;
    sigset_t suspend_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask , SIGUSR2);
    sigaddset(&block_mask , SIGALRM);
    if(sigprocmask(SIG_BLOCK , &block_mask , &old_mask) == -1){
        return 1;
    }


    struct sigaction sa_usr2;
    sa_usr2.sa_handler = &sigusr2_handler;
    sa_usr2.sa_flags = 0;
    sigemptyset(&sa_usr2.sa_mask);
    if(sigaction(SIGUSR2 , &sa_usr2 , NULL) == -1){
         perror("Failed to register the SIGUSR2 signal using sigaction");
         return -1;
    }

    struct sigaction sa_timeout;
    sa_timeout.sa_handler = &sig_timeout_handler;
    sa_timeout.sa_flags = 0;
    sigemptyset(&sa_timeout.sa_mask);
    if(sigaction(SIGALRM , &sa_timeout , NULL) == -1){
         perror("Failed to register the SIGALARM signal using sigaction");
         return -1;
    }
    if(kill(server_pid , SIGUSR1) == -1){
        perror("Failed to send SIGUSR1 signal to server");
        return 1;
    }
    suspend_mask = old_mask;
    sigdelset(&suspend_mask , SIGUSR2);
    sigdelset(&suspend_mask , SIGALRM);
    

    alarm(1); //This is used for telling whether the client waits for the login reply from server for too long
    while(!sigusr2_flag && !timeout_flag){
        //This will allow the process to sleep, until receiving a non-block signal
        sigsuspend(&suspend_mask);
    }
    alarm(0);

    //This will block the SIGUSR2 and SIGALARM
    //Just in case the client has not yet sigsuspend, but the signal is coming
    if(sigprocmask(SIG_SETMASK , &old_mask , NULL) == -1){
        perror("Failed to restore the signal mask");
        return 1;
    }
    if(timeout_flag && !sigusr2_flag){
        fprintf(stderr , "connection Timeout");
        return 1;
    }

    pid_t client_pid = getpid();
    char fifo_C2S_path[PIPE_PATH_MAX_SIZE];
    char fifo_S2C_path[PIPE_PATH_MAX_SIZE];

    snprintf(fifo_C2S_path , PIPE_PATH_MAX_SIZE , "FIFO_C2S_%d" , client_pid);
    snprintf(fifo_S2C_path , PIPE_PATH_MAX_SIZE , "FIFO_S2C_%d" , client_pid);
    
    int C2S_fd = open(fifo_C2S_path , O_WRONLY);
    if(C2S_fd == -1){
        perror("failed to open the named pipe from client to server");
        return 1;
    }
    int S2C_fd = open(fifo_S2C_path , O_RDONLY | O_NONBLOCK);
    if(S2C_fd == -1){
        perror("failed to open the named pipe from server to client");
        close(C2S_fd);
        return 1;
    }

    *c2s_fd = C2S_fd;
    *s2c_fd = S2C_fd;
    return 0;
}
