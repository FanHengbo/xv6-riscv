#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2

int
main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(STDERR, "Usage: pingpong\n");
        exit(1);
    }
    int parentToChild[2];
    int childToParent[2];
    char buf = 'c';
    int pid;

    pipe(parentToChild);
    pipe(childToParent);
    if ((pid = fork()) == 0) {
        close(parentToChild[1]);
        close(childToParent[0]);
        if ((read(parentToChild[0], &buf, 1)) != 1) {
            fprintf(2, "child read error\n");
            exit(1);
        }
        close(parentToChild[0]);

        printf("%d: received ping\n", getpid());
        if ((write(childToParent[1], &buf, 1)) != 1) {
            fprintf(2, "child write error\n");
        }
        close(childToParent[1]);
        exit(0);
    }
    close(parentToChild[0]);
    close(childToParent[1]);
    if ((write(parentToChild[1], &buf, 1)) != 1) {
        fprintf(2, "parent write error\n");
        exit(1);
    }
    close(parentToChild[1]);
    if ((read(childToParent[0], &buf, 1)) != 1) {
        fprintf(2, "parent read error\n");
        exit(1);
    }
    printf("%d: received pong\n", getpid());
    close(childToParent[0]);
    wait(&pid);
    exit(0);
}