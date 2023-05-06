#include "operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#define S 20

int initialize_server(int argc, char **pipename);

void mount_func(int id, char* pipename);
void unmount_func(int session_id);
void open_func(int session_id, char *filename, int flags);
void close_func(int session_id, int fd);
void write_func(int session_id, int fd, size_t len, char *buffer);
void read_func(int session_id, int fd, size_t len);
void shutdown_func(int session_id);

void send_msg(int pipe, char *str);
/*void send_msg_err(int pipe, int to_send);*//*send message with error equal to to_send*/
void send_msg_int(int pipe, int to_send);
void send_msg_new(int pipe, char *buffer, size_t len);
int read_msg(int pipe, char* buffer, size_t len);

void* receive_request();
void* func_worker(void* args);
void process_request(int id);


typedef struct session{
    int pipe;
    pthread_mutex_t lock_cond;
    int requests;
    int used;
    char *buffer_requests;
    pthread_cond_t var_cond;
    pthread_rwlock_t this_lock;
}*session_info;

int this_server_fd;
session_info all_sessions[S];

int count_sessions = 0;
pthread_rwlock_t count_sessions_lock = PTHREAD_RWLOCK_INITIALIZER;

int main(int argc, char **argv) {
    int temp;
    
    temp = initialize_server(argc, argv);
    if(temp != 0){
        return temp;
    }

    pthread_t tid[S + 1];

    if(pthread_create(&tid[S], NULL, receive_request, NULL) != 0){
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < S; i++){
        void *arg = (void*)malloc(sizeof(int));
        memcpy(arg, &i, sizeof(int));
        if(pthread_create(&tid[i], NULL, func_worker, arg) != 0){
            exit(EXIT_FAILURE);
        }
    }

    pthread_join(tid[S], NULL);
    for(int i = 0; i < S; i++){
        pthread_join(tid[i], NULL);
    }


    return 0;
}

void mount_func(int id, char *buffer){
    int client_pipe;

    sleep(1);/*wait for client to open pipe*/
    client_pipe = open(buffer, O_WRONLY);

    if(id == -1){
        send_msg_int(client_pipe, -1);
    }else{
        all_sessions[id]->pipe = client_pipe;
        send_msg_int(client_pipe, id);
    }
}


void unmount_func(int session_id){
    int client_pipe;
    if(all_sessions[session_id]->used != false){
        client_pipe = all_sessions[session_id]->pipe;
        
        all_sessions[session_id]->pipe = -1;
        all_sessions[session_id]->used = false;

        pthread_rwlock_wrlock(&count_sessions_lock);
        count_sessions--;
        pthread_rwlock_unlock(&count_sessions_lock);

        send_msg_int(client_pipe, 0);
        close(client_pipe);
    }else{
        /*TODO:*/
    }
    
}

void open_func(int session_id, char *filename, int flags){
    int client_pipe = all_sessions[session_id]->pipe;

    int fd = tfs_open(filename, flags);
    send_msg_int(client_pipe, fd);

}

void close_func(int session_id, int fd){
    int client_pipe = all_sessions[session_id]->pipe;

    int result = tfs_close(fd);

    send_msg_int(client_pipe, result);
}

void write_func(int session_id, int fd, size_t len, char *buffer){
    int client_pipe = all_sessions[session_id]->pipe;
    
    int result = (int) tfs_write(fd, buffer, len);
    send_msg_int(client_pipe, result);
}


void read_func(int session_id, int fd, size_t len){
    int client_pipe;
    size_t read_ = 0;
    int bytes_read;
    char* buffer = (char*)malloc(len);
    char *buffer_msg;


    bytes_read = (int) tfs_read(fd, buffer, len);
    buffer_msg = (char*)malloc(sizeof(int) + sizeof(char) * (size_t)bytes_read);

    memcpy(buffer_msg, &bytes_read, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, buffer, sizeof(char) * (size_t)bytes_read);
    read_ += sizeof(char) * (size_t)bytes_read;

    client_pipe = all_sessions[session_id]->pipe;

    send_msg_new(client_pipe, buffer_msg, read_);

}


void shutdown_func(int id){
    int client_pipe;

    client_pipe = all_sessions[id]->pipe;

    int msg = tfs_destroy_after_all_closed();

    close(this_server_fd);

    send_msg_int(client_pipe, msg);

    exit(0);
}





void send_msg(int pipe, char *str){
    size_t len = strlen(str);
    size_t written = 0;

    while (written < len) {
        ssize_t ret = write(pipe, str + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        written += (size_t)ret;
    }
}
/*
void send_msg_err(int pipe, int to_send){
    char *msg = (char*)malloc(sizeof(char)*3);
    sprintf(msg, "%d", to_send);

    send_msg(pipe, msg);
}*/

void send_msg_int(int pipe, int to_send){
    char *msg = (char*)malloc(sizeof(int));
    memcpy(msg, &to_send, sizeof(int));

    send_msg_new(pipe, msg, sizeof(int));
    free(msg);
}

void send_msg_new(int pipe, char *buffer, size_t len){
    ssize_t ret = write(pipe, buffer, len);
    if (ret < 0) {
        fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int read_msg(int pipe, char* buffer, size_t len){
    while(true){
        ssize_t ret = read(pipe, buffer, len);
        if (ret == 0 || ret == len) {
            return 0;
        }else if (ret == -1){
            return -1;
        }else{
            len -= (size_t) ret;
        }
    }
}

int initialize_server(int argc, char ** argv){
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if(tfs_init() != 0){
        return -1;
    }

    if(unlink(pipename) != 0 && errno != ENOENT){
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(pipename, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    this_server_fd = open(pipename, O_RDONLY);
    if(this_server_fd == -1){
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < S; i++){
        all_sessions[i] = (session_info)malloc(sizeof(struct session));
        pthread_mutex_init(&all_sessions[i]->lock_cond, NULL);
        all_sessions[i]->requests = 0;
        all_sessions[i]->used = false;
        all_sessions[i]->buffer_requests = NULL;
    }
    return 0;
}

void *receive_request(){
    while (true){
        char op_code;
        int id = -1;


        
        ssize_t ret = read(this_server_fd, &op_code, sizeof(char));
        if(ret > 0){

            if(op_code != '1'){
                char *received = (char*)malloc(sizeof(int));
                read_msg(this_server_fd, received, sizeof(int));
                memcpy(&id, received, sizeof(int));
            }


            switch(op_code){
            case '1':
                if(count_sessions >= S){
                    id = -1;
                }else{
                    for(int i = 0; i < S; i++){
                        if(all_sessions[i]->used == false){
                            id = i;
                            all_sessions[i]->used = true;
                            break;
                        }
                    }
                }
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char) * 40 + sizeof(char) + sizeof(int));
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                read_msg(this_server_fd, all_sessions[id]->buffer_requests + sizeof(char), sizeof(char) * 40);
                break;
            case '2':
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char));
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                break;
            case '3':
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char) * 40 + sizeof(char) + sizeof(int));
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                read_msg(this_server_fd, all_sessions[id]->buffer_requests + sizeof(char), sizeof(char) * 40 + sizeof(int));
                break;
            case '4':
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char) + sizeof(int));  
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                read_msg(this_server_fd, all_sessions[id]->buffer_requests + sizeof(char), sizeof(int));
                break;
            case '5': ;
                char *temp = (char*)malloc(sizeof(char) + sizeof(int) + sizeof(size_t));
                size_t len, read_ = 0;

                memcpy(temp, &op_code, sizeof(char));
                read_ += sizeof(char);

                read_msg(this_server_fd, temp + read_, sizeof(int) + sizeof(size_t));
                read_ += sizeof(int) + sizeof(size_t);
                memcpy(&len, temp + sizeof(char) + sizeof(int), sizeof(size_t));

                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char) + sizeof(int) + sizeof(size_t) + len);
                memcpy(all_sessions[id]->buffer_requests, temp, read_);
                read_msg(this_server_fd, all_sessions[id]->buffer_requests + read_, len);
                break;
            case '6':
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char) + sizeof(int) + sizeof(size_t));  
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                read_msg(this_server_fd, all_sessions[id]->buffer_requests + sizeof(char), sizeof(int) + sizeof(size_t));
                break;
            case '7':
                all_sessions[id]->buffer_requests = (char*)malloc(sizeof(char));
                memcpy(all_sessions[id]->buffer_requests, &op_code, sizeof(char));
                break;
            default:
                break;
            }

            pthread_mutex_lock(&all_sessions[id]->lock_cond);
            all_sessions[id]->requests++;

            pthread_cond_signal(&all_sessions[id]->var_cond);
            pthread_mutex_unlock(&all_sessions[id]->lock_cond);
        }
    }
    
}

void* func_worker(void* args){
    int this_session_id;
    memcpy(&this_session_id, args, sizeof(int));

    free(args);

    while (true){
        pthread_mutex_lock(&all_sessions[this_session_id]->lock_cond);


        while(all_sessions[this_session_id]->requests == 0){
            pthread_cond_wait(&all_sessions[this_session_id]->var_cond, &all_sessions[this_session_id]->lock_cond);
        }

        process_request(this_session_id);

        all_sessions[this_session_id]->requests--;
        free(all_sessions[this_session_id]->buffer_requests);

        pthread_mutex_unlock(&all_sessions[this_session_id]->lock_cond);

    }

    return NULL;
}

void process_request(int id){
    char op_code;
    size_t read_ = 0;

    memcpy(&op_code, all_sessions[id]->buffer_requests, sizeof(char));
    read_ += sizeof(char);

    int fd;
    size_t len;

    switch (op_code){
    case '1': ;
        char *pipename = (char*)malloc(sizeof(char) * 40);
        memcpy(pipename, all_sessions[id]->buffer_requests + read_, sizeof(char) * 40);
        read_ += sizeof(char) * 40;
        int id_verification;
        memcpy(&id_verification, all_sessions[id]->buffer_requests + read_, sizeof(int));
        if(id_verification == -1){
            printf("debug erro id mount\n");
        }
        mount_func(id, pipename);
        break;
    case '2':
        unmount_func(id);
        break;
    case '3': ;
        char *filename = (char*)malloc(sizeof(char) * 40);
        int flags;
        memcpy(filename, all_sessions[id]->buffer_requests + read_, sizeof(char) * 40);
        read_ += sizeof(char) * 40;
        memcpy(&flags, all_sessions[id]->buffer_requests + read_, sizeof(int));
        open_func(id, filename, flags);
        break;
    case '4': ;
        memcpy(&fd, all_sessions[id]->buffer_requests + read_, sizeof(int));
        close_func(id, fd);
        break;
    case '5': ;

        memcpy(&fd, all_sessions[id]->buffer_requests + read_, sizeof(int));
        read_ += sizeof(int);

        memcpy(&len, all_sessions[id]->buffer_requests + read_, sizeof(size_t));
        read_ += sizeof(size_t);
        char *buffer = (char*)malloc(sizeof(char) * len);

        memcpy(buffer, all_sessions[id]->buffer_requests + read_, len);

        write_func(id, fd, len, buffer);
        break;
    case '6':

        memcpy(&fd, all_sessions[id]->buffer_requests + read_, sizeof(int));
        read_ += sizeof(int);

        memcpy(&len, all_sessions[id]->buffer_requests + read_, sizeof(size_t));
        
        read_func(id, fd, len);
        break;
    case '7':
        shutdown_func(id);
        break;
    default:
        break;
    }
}