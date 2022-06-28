#include "listImpl.h"
#include "thread.h"
#include <time.h>
#include <stdlib.h>
// spinlock_t rlock;
#define MAX (1 << 17)
#define RATIO 1
int nalloc;
mutex_t lk;
int count;
cond_t cv;
void *buffer[MAX];
int fill_ptr;
int use_ptr;
int ploops;
int cloops;
int sum;
int cnt;
uintptr_t hh, tt;
int* table;
void put(void *ptr)
{
  // mutex_lock(&lk);
  buffer[fill_ptr] = ptr;
  // printf("%d\n", table[(uintptr_t)ptr - HEAP_START]);
  assert(table[(uintptr_t)ptr - hh] == 0);
  table[(uintptr_t)ptr - hh] += 1;
  fill_ptr = (fill_ptr + 1) % MAX;
  count++;
  cond_signal(&cv);
  mutex_unlock(&lk);
}

void get() {
  void *tmp = buffer[use_ptr];
  table[(uintptr_t)tmp - hh] = 0;
  use_ptr = (use_ptr + 1) % MAX;
  count--;
  // printf("free %p\n", tmp);
  kfree(tmp);
  assert(table[(uintptr_t)tmp - hh] == 0);
}

void Tproducer()
{
  for (int i = 0; i < ploops; i++) {
      mutex_lock(&lk);
      cnt++;
      int sz = rand() % (1 << 4) + 1;
      void *ptr = kalloc(sz);
      assert(ptr != NULL);
      // printf("allocate %p\n", ptr);
      put(ptr);
  }
}

void Tconsumer() {
  for (int i = 0; i < cloops; i++) {
    mutex_lock(&lk);
    cnt++;
      // printf("sleep\n");
    while (count == 0)
      cond_wait(&cv, &lk);
    assert(count != 0);
    get();
    cond_signal(&cv);
    mutex_unlock(&lk);
  }
}

// void alloc_and_free() {
//     void *ptr = kalloc(10);
//     for (int i = 0; i < loops * 2 ; i++)
//     {
//       sum++;
//       kfree(ptr);
//       ptr = (void *)kalloc(10);
//     }
//     kfree(ptr);
// }

int main(int argc, char* argv[]) {
  assert(argc == 2);
  nalloc = atoi(argv[1]);
  srand(time(NULL));
  pmm_init();
  table = (int*)malloc((tt - hh) * sizeof(int));
  int consumers = nalloc / (RATIO + 1);
  int producers = nalloc - consumers;
  ploops = MAX / producers;
  cloops = MAX / consumers;
  //测试并发allocate/free
  for (int i = 0; i < producers; i++)
  {
    create(Tproducer);
  }
  //join();
  for (int i = 0; i < consumers; i++)
  {
      create(Tconsumer);
  }
  join();
  // uintptr_t ptr = (uintptr_t)kalloc(1);
  // assert(ptr == HEAP_START);
  printf("done\n"); // kfree(ptr);
                    // for (int i = 0; i < nalloc; i++)
                    // {
                    //   create(alloc_and_free);
                    // }
                    // join();
                    // void* ptr1 = (uintptr_t)kalloc(1);
                    // assert(ptr1 == HEAP_START);
}