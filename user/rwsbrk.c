#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"


int
rwsbrk()
{
  int fd, n;
  
  uint64 a = (uint64) sbrk(8192);

  if(a == 0xffffffffffffffffLL) {
    printf("sbrk(rwsbrk) failed\n");
    return -1;
  }
  
  if ((uint64) sbrk(-8192) ==  0xffffffffffffffffLL) {
    printf("sbrk(rwsbrk) shrink failed\n");
    return -1;
  }

  fd = open("rwsbrk", O_CREATE|O_WRONLY);
  if(fd < 0){
    printf("open(rwsbrk) failed\n");
    return -1;
  }
  n = write(fd, (void*)(a+4096), 1024);
  if(n >= 0){
    printf("write(fd, %p, 1024) returned %d, not -1\n", a+4096, n);
    return -1;
  }
  close(fd);
  unlink("rwsbrk");

  fd = open("README", O_RDONLY);
  if(fd < 0){
    printf("open(rwsbrk) failed\n");
    return -1;
  }
  n = read(fd, (void*)(a+4096), 10);
  if(n >= 0){
    printf("read(fd, %p, 10) returned %d, not -1\n", a+4096, n);
    return -1;
  }
  close(fd);
  

  //printf("Sucess\n");
  return 0;
}

int main() {
    int a = rwsbrk();
    if (a == 0)
        printf("Sucess\n");
    exit(0);
}