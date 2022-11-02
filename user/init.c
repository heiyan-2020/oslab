#include "ulib.h"

#define DEST  '+'
#define EMPTY '.'
#define N 10000

struct move {
  int x, y, ch;
} moves[] = {
  { 0, 1, '>' },
  { 1, 0, 'v' },
  { 0, -1, '<' },
  { -1, 0, '^' },
};

void display();

void dfs(int x, int y, char map[][16]) {
  if (map[x][y] == DEST) {
    display(map);
  } else {
    int nfork = 0;
    for (struct move *m = moves; m < moves + 4; m++) {
      int x1 = x + m->x, y1 = y + m->y;
      if (map[x1][y1] == DEST || map[x1][y1] == EMPTY) {
        int pid = fork();
        if (pid == 0) { // map[][] copied
          map[x][y] = m->ch;
          dfs(x1, y1, map);
          exit(0); // clobbered map[][] discarded
        } else {
          nfork++;
          wait(0); // wait here to serialize the search
        }
      }
    }

    while (nfork--) wait(0);
  }
}

void display(char map[][16]) {
  for (int i = 0; ; i++) {
    for (const char *s = map[i]; *s; s++) {
      switch (*s) {
        case EMPTY: printf("   "); break;
        case DEST : printf(" * "); break;
        case '>'  : printf(" > "); break;
        case '<'  : printf(" < "); break;
        case '^'  : printf(" ^ "); break;
        case 'v'  : printf(" v "); break;
        default   : printf("###"); break;
      }
    }
    printf("\n");
    if (strlen(map[i]) == 0) break;
  }
  sleep(1); // to see the effect of parallel search
}

int fork_test_1(int n) {
  for (int i = 0; i < n; i++) {
    fork();
    printf("1");
  }
  return 0;
}

void fork_wait_test() {
  int pid = fork();
  if (pid == 0) {
    printf("Hello, ");
  } else {
    int x = 0;
    wait(&x);
    printf("world, x=%d\n", x);
  }
  exit(100);
}

void stress_test() {
  while (1) {
    int pid = fork();
    if (pid == 0) {
      exit(0);
    } else {
      if (pid % 10000 == 0) {
        printf("pid = %d\n", pid);
      }
    }
  }
}

void forktest()
{
  int n, pid;

  printf("fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit(0);
  }

  if(n == N){
    printf("fork claimed to work N times!\n");
    exit(1);
  }

  for(; n > 0; n--){
    if(wait(0) < 0){
      printf("wait stopped early\n");
      exit(1);
    }
  }

  if(wait(0) != -1){
    printf("wait got too many\n");
    exit(1);
  }

  printf("fork test OK\n");
}

#define PMEM_LIMIT (1 << 27)
#define PAGESIZE 4096
//先给一个线程分配物理内存一半大小的内存(并通过写入每个页面保证分配)
//如果支持cow，就能成功fork，否则将在fork时拷贝页面导致故障
void cow_test1() {
  int len = PMEM_LIMIT >> 1;
  void *start = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE);
  for (void *itr = start; itr < start + len; itr += PAGESIZE) {
    int *writable = (int *)itr;
    *writable = 1;
  }

  fork();

  printf("done\n");
}

//测试munmap
//mmap -> 写入触发分配 -> munmap释放
//如果munmap可以正常释放，那么这个循环就能一直进行下去
void mmap_test1() {
  while (1) {
    int len = PMEM_LIMIT >> 7; //1MB
    void *start = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE);
    for (void *itr = start; itr < start + len; itr += PAGESIZE) {
      int *writable = (int *)itr;
      *writable = 1;
    }
    
  }
}

void mmap_test2() {
  int *shared_space = (int *)mmap(NULL, 4096, PROT_WRITE | PROT_READ, MAP_SHARED);
  int *private_space = (int *)mmap(NULL, 4096, PROT_WRITE | PROT_READ, MAP_PRIVATE);
  int pid = fork();
  if (pid == 0) {
    *shared_space = 100;
    *private_space = 1000;
    exit(0);
  } else {
    wait(NULL);
    printf("*shared_space = %d\n", *shared_space);
    printf("*private_space = %d\n", *private_space);
    *private_space = 100;
  }
}

int main() {
  // printf("Hello from user space\n");
  fork_test_1(10);
  // fork_wait_test();
  // char map[][16] = {
  //   "#######",
  //   "#.#.#+#",
  //   "#.....#",
  //   "#.....#",
  //   "#...#.#",
  //   "#######",
  //   "",
  // };
  // dfs(1, 1, map);
  // 
  // cow_test1();
  // stress_test();
  // mmap_test2();
  return 0;
}
