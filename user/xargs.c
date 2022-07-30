#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2

int main (int argc, char *argv[]) {
    int i;
    int pid;
    char *combinedArgv[MAXARG];
    char c;
    char line[512];
    char *p = line;
    char **argvPtr = combinedArgv;

    if (argc < 2) {
        fprintf(STDERR, "Usage: <command> <args> | xargs <command> <args>");
        exit(1);
    }
    for (i = 1; i < argc; i++) {
        *argvPtr++ = argv[i];
    }
    while (read(STDIN, &c, 1)) {
        *p++ = c;
        if (c == '\n') {
            *(p-1) = '\0';
            *argvPtr = line;
            if ((pid = fork()) == 0) {
                exec(combinedArgv[0], combinedArgv);
            }
            wait(&pid);
            p = line;
            argvPtr = combinedArgv+argc-1;
        }
    }
    exit(0);
}