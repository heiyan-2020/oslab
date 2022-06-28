#include "ulib.h"

#define FORK_TIMES 10000000

void pid_test() {
  printf("pid = %d\n", getpid());
}

void hello_test() {
  int pid = fork(), x=0;
  const char *fmt;
  if (pid) {
    fmt = "Parent #%d\n";
  } else {
    sleep(1);
    fmt =  "Child #%d\n";
  }
  while (1) {
    printf(fmt, ++x); 
    // printf("uptime=%d\n", uptime());
    sleep(2);
  }
}

void fork_test() {
  for (int i = 0; i < FORK_TIMES; i++) {
    int pid = fork();
    if (pid == 0) {
      volatile int x = 0;
      while (1) {
        printf("%d\n", x++);
      }
    }
  }
}

int main() {
  // Example:
  printf("Hello from user space\n");

  // pid_test();
  // hello_test();
  fork_test();
  return 0;
}
