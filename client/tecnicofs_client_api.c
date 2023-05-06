#include "tecnicofs_client_api.h"

int this_id = UNINITIALIZED_VALUE;
int fd_server_pipe = UNINITIALIZED_VALUE;
int this_session_pipe = UNINITIALIZED_VALUE;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(char)*40);
    size_t read_ = 0;
    
    int result;
    char *received = (char*)malloc(sizeof(int));

    if(unlink(client_pipe_path) != 0 && errno != ENOENT){
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        return -1;
    }

    if (mkfifo(client_pipe_path, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return -1;
    }

    fd_server_pipe = open(server_pipe_path, O_WRONLY);
    if(fd_server_pipe == -1){
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
    }

    char op_code = TFS_OP_CODE_MOUNT + '0';
    memcpy(buffer_msg + read_, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, (char*) client_pipe_path, (sizeof(char) * strlen(client_pipe_path)));
    read_ += (sizeof(char) * strlen(client_pipe_path));

    send_msg_new(fd_server_pipe, buffer_msg, sizeof(char) + sizeof(char)*40);

    this_session_pipe = open(client_pipe_path, O_RDONLY);
    if(this_session_pipe == -1){
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
    }

    if(read_msg(this_session_pipe, received, sizeof(int)) != 0){
        return -1;
    }

    memcpy(&result, received, sizeof(int));
    if(result == -1){
        return -1;
    }else{
        this_id = result;
        return 0;
    }

}

int tfs_unmount() {
    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(int));
    size_t read_ = 0;
    char *received = (char*)malloc(sizeof(int));
    int result;

    if(fd_server_pipe == -1){
        return -1;
    }

    char op_code = TFS_OP_CODE_UNMOUNT + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    send_msg_new(fd_server_pipe, buffer_msg, read_);
    
    read_msg(this_session_pipe, received, sizeof(int));

    memcpy(&result, received, sizeof(int));

    if(result == -1){
        return -1;
    }
    this_id = UNINITIALIZED_VALUE;

    close(this_session_pipe);/*ta a funcionar*/
    close(fd_server_pipe);

    this_session_pipe = UNINITIALIZED_VALUE;
    fd_server_pipe = UNINITIALIZED_VALUE;

    return 0;
}


int tfs_open(char const *name, int flags){
    char* buffer_msg = (char*)malloc(sizeof(char) + sizeof(int) + sizeof(char)*40 + sizeof(int));
    char *received = (char*)malloc(sizeof(int));
    size_t read_ = 0;
    int result;

    if(fd_server_pipe == -1){
        return -1;
    }

    char op_code = TFS_OP_CODE_OPEN + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    char *name_aux = (char*)malloc(sizeof(char)*40);
    memcpy(name_aux, name, sizeof(char) * 40);
    memcpy(buffer_msg + read_, name_aux, sizeof(char)*40);
    read_ += sizeof(char) * 40;

    memcpy(buffer_msg + read_, &flags, sizeof(int));
    read_ += sizeof(int);
    
    send_msg_new(fd_server_pipe, buffer_msg, read_);

    read_msg(this_session_pipe, received, sizeof(int));
    memcpy(&result, received, sizeof(int));

    return result;
}

int tfs_close(int fhandle) {
    if(fd_server_pipe == -1){
        return -1;
    }

    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(int) * 2);
    size_t read_ = 0;
    int result;
    char *received = (char*)malloc(sizeof(int));

    char op_code = TFS_OP_CODE_CLOSE + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, &fhandle, sizeof(int));
    read_ += sizeof(int);

    send_msg_new(fd_server_pipe, buffer_msg, read_);

    read_msg(this_session_pipe, received, sizeof(int));
    memcpy(&result, received, sizeof(int));

    return result;
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    if(fd_server_pipe == -1){
        return -1;
    }
    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(int) * 2 + sizeof(size_t) + sizeof(char) * len);
    size_t read_ = 0;
    char *received = (char*)malloc(sizeof(int));
    int result;

    char op_code = TFS_OP_CODE_WRITE + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, &fhandle, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, &len, sizeof(size_t));
    read_ += sizeof(size_t);

    memcpy(buffer_msg + read_, buffer, len);
    read_ += len;

    send_msg_new(fd_server_pipe, buffer_msg, read_);

    if(read_msg(this_session_pipe, received, sizeof(int)) != 0){
        return -1;
    }

    memcpy(&result, received, sizeof(int));

    return result;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    if(fd_server_pipe == -1){
        return -1;
    }

    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(int)*2 + sizeof(size_t));
    size_t read_ = 0;
    int bytes_read;

    char op_code = TFS_OP_CODE_READ + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, &fhandle, sizeof(int));
    read_ += sizeof(int);

    memcpy(buffer_msg + read_, &len, sizeof(size_t));
    read_ += sizeof(size_t);

    send_msg_new(fd_server_pipe, buffer_msg, read_);

    char* received = (char*)malloc(sizeof(int));

    read_msg(this_session_pipe, received, sizeof(int));
    memcpy(&bytes_read, received, sizeof(int));

    read_msg(this_session_pipe, buffer,(size_t) bytes_read);

    return bytes_read;

}


int tfs_shutdown_after_all_closed() {
    if(fd_server_pipe == -1){
        return -1;
    }
    
    char *buffer_msg = (char*)malloc(sizeof(char) + sizeof(int));
    size_t read_ = 0;

    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED + '0';
    memcpy(buffer_msg, &op_code, sizeof(char));
    read_ += sizeof(char);

    memcpy(buffer_msg + read_, &this_id, sizeof(int));
    read_ += sizeof(int);

    send_msg_new(fd_server_pipe, buffer_msg, read_);

    int result;
    char *msg = (char*)malloc(sizeof(char));

    memcpy(&result, msg, sizeof(int));
    return result;
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



int result_read(){
    while(true){
        char* msg = (char*)malloc(sizeof(int));
        ssize_t ret = read(this_session_pipe, msg, sizeof(int));
        if(ret > 0){
            return atoi(msg);
        }
    }
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


