#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv){
    if(argc < 3){
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }
    /*
    char *path = "/f1";
    char str[60];
    memset(str, 'A', 59);
    str[59] = '\0';

    char str2[60];*/

    sleep(1);

    assert(tfs_mount(argv[1], argv[2]) == 0);
    /*
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, str, 60) == 60);
    assert(tfs_close(fd) == 0);
    int fd2 = tfs_open(path, 0);
    assert(fd2 != -1);
    assert(tfs_read(fd2, str2, 60) == 60);
    assert(!strcmp(str,str2));
    assert(tfs_unmount() == 0);
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, path, strlen(path)) == strlen(path));
    tfs_read(fd, path, strlen(path));
    assert(tfs_unmount() == 0);*/

    printf("Successful test\n");

    return 0;
}