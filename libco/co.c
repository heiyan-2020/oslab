#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <assert.h>
#define STACK_SIZE 65536
#define POOL_SIZE 128
#define ROUNDUP(a, sz)      ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
int a[STACK_SIZE];
enum co_status {
  CO_NEW = 1, // 新创建，还未执行过
  CO_RUNNING, // 执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
  char *name;
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;  // 协程的状态
  struct co *    waiter;  // 是否有其他协程在等待当前协程
  jmp_buf        context; // 寄存器现场 (setjmp.h)
  uint8_t        stack[STACK_SIZE]; // 协程的堆栈
};
typedef struct co CO;

CO *co_pool[POOL_SIZE];        //协程池, 用于保存当前占用着资源的协程.
int co_current; //当前正在运行的协程在池中编号

__attribute__((optimize("O0"))) void wrapper() {
  co_pool[co_current]->status = CO_DEAD;
}

void __attribute__((optimize("O0"))) stack_switch_call(uintptr_t sp, void *entry, uintptr_t arg) {
  asm volatile(
#if __x86_64__
      "movq %0, %%rsp; movq %2, %%rdi;callq *%1;"
      :
      : "b"((uintptr_t)sp), "d"(entry), "a"(arg)
      : "memory"
#else
      "movl %0, %%esp; movl %2, (%0); call *%1;"
      :
      : "b"((uintptr_t)sp), "d"(entry), "a"(arg)
      : "memory"
#endif
  );
  wrapper();
  co_yield();
}

CO *co_start(const char *name, void (*func)(void *), void *arg)
{
  CO* co = (CO *)malloc(sizeof(CO));
  co->func = func, co->arg = arg, co->status = CO_NEW;

  for (int i = 0; i < POOL_SIZE; i++) {
    if (co_pool[i] == NULL) {
      co_pool[i] = co;
      break;
    }
  }
  return co;
}

void co_wait(CO *co) {
  //wrapper();
  co_pool[co_current]->status = CO_WAITING;
  co->waiter = co_pool[co_current];
  // co未结束则一直等待
  while (co->status != CO_DEAD) {
    co_yield();
  }
  //释放资源
  co_pool[co_current]->status = CO_RUNNING;
  for (int i = 0; i < POOL_SIZE; i++)
  {
    if (co_pool[i] == co) {
      free(co);
      co_pool[i] = NULL;
      return;
    }
  }
  assert(0);
}

void co_yield() {
  int val = setjmp(co_pool[co_current]->context);
  if (val == 0) {
    //当前协程调用yield(), 需要切换到下一个协程
    int next_co = rand() % POOL_SIZE;
    while (next_co == co_current || co_pool[next_co] == NULL || co_pool[next_co]->status == CO_DEAD) {
      next_co = (next_co + 1) % POOL_SIZE;
    }
    co_current = next_co;
    if (co_pool[co_current]->status == CO_NEW) {
      co_pool[co_current]->status = CO_RUNNING;
      stack_switch_call(ROUNDUP((co_pool[co_current]->stack + STACK_SIZE * sizeof(uint8_t)), 16), co_pool[co_current]->func, (uintptr_t)co_pool[co_current]->arg);
    } else {
      longjmp(co_pool[co_current]->context, 1);
    }
  }
  else
  {
    //切换回来，直接返回
    return;
  }
}

//初始化
__attribute__((constructor)) void co_init() {
  //初始化协程池
  
  //加入main协程
  CO* co = (CO *)malloc(sizeof(CO));
  co->func = NULL, co->arg = NULL, co->status = CO_RUNNING;
  co_pool[0] = co;
}
