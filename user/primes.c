#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2
void CheckAndSend() {
    int leftNeighbor;
    int rightNeighbor;
    int length = sizeof(int);
    int fd[2];
    int pid;
    pipe(fd);
    if (read(STDIN, &leftNeighbor, length)) {
        printf("prime %d\n", leftNeighbor);
    }
    else
        return;

    if ((pid = fork()) == 0) {
        close(fd[1]);
        close(STDIN);
        dup(fd[0]);
        close(fd[0]);
        CheckAndSend();
    }
    else {
        close(fd[0]);
        while (read(STDIN, &rightNeighbor, length)) {
            if (rightNeighbor % leftNeighbor) {
                write(fd[1], &rightNeighbor, length);
            }
        }
        close(fd[1]);
        wait(&pid);
    }
    exit(0);
}
int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "Usage: primes\n");
        exit(1);
    }
    int pid, fd[2], i;
    int length = sizeof(int);
    pipe(fd);
    if ((pid = fork()) == 0) {
        close(fd[1]);
        close(STDIN);
        dup(fd[0]);
        close(fd[0]);
        CheckAndSend();
    }
    else {
        close(fd[0]);
        close(STDOUT);
        dup(fd[1]);
        close(fd[1]);
        close(STDIN);
        for (i = 2; i <= 35; i++) {
            write(STDOUT, &i, length);
        }
        close(STDOUT);
        wait(&pid);
    }
    exit(0);
}