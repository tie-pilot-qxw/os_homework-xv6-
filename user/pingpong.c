#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 1) {
        fprintf(2, "usage: pingpong\n");
        exit(1);
    }
    int p[2];
    char buf[100];
    pipe(p);
    int pid = fork();
    if(pid > 0){
        if (write(p[1], "ping", sizeof("ping")) != sizeof("ping")){
            close(p[1]);
            fprintf(2,"write error\n");
            exit(1);
        }
        close(p[1]);
        if (read(p[0], buf, sizeof("pong")) < 0){
            fprintf(2,"read error\n");
            exit(1);
        }
        printf("%d:received %s\n", getpid(), buf);
        pid = wait((int *) 0);
    } else if(pid == 0){
        if (read(p[0], buf, sizeof(buf)) < 0) {
            fprintf(2, "read error\n");
            exit(1);
        }
        printf("%d:received %s\n", getpid(), buf);
        if (write(p[1], "pong", sizeof("pong")) != sizeof("pong")) {
            close(p[1]);
            fprintf(2,"write error\n");
            exit(1);
        }
        close(p[1]);
        exit(0);
    } else {
        printf("fork error\n");
    }
    close(p[0]);
    close(p[1]);
    exit(0);
}