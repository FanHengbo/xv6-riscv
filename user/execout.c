#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
execout()
{
  for(int avail = 0; avail < 15; avail++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      return -1;
    } else if(pid == 0){
      // allocate all of memory.
      while(1){
        uint64 a = (uint64) sbrk(4096);
        if(a == 0xffffffffffffffffLL)
          break;
        *(char*)(a + 4096 - 1) = 1;
      }

      // free a few pages, in order to let exec() make some
      // progress.
      for(int i = 0; i < avail; i++)
        sbrk(-4096);
      
      close(1);
      char *args[] = { "echo", "x", 0 };
      exec("echo", args);
      exit(0);
    } else {
      wait((int*)0);
    }
  }

  return 0;
}

int main() {
    int a = execout();
    if (!a) {
        printf("Sucess\n");
    }
    return 0;
}