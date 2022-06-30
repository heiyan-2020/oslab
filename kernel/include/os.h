#ifndef OS_H
#define OS_H

#include <stdint.h>
#include <param.h>
#include <klib.h>
#include <klib-macros.h>
#include <kernel.h>

enum thread_state {
  RUNNING = 0, RUNNABLE, SLEEPY, SLEEPING, BLOCKED, DEAD
};

typedef struct {
  int    cpuno; //cpu_no
  int    noff; //Depth of push_off() nesting.
  int    intena; //Were interrupts enabled before push_off()?
  task_t *cur;
  task_t *prev;
} cpu_t;

struct spinlock {
  int locked;
  const char *name; //debug
  cpu_t *cpu;
  int line;
};

typedef struct {
    task_t      *head;
    task_t      *rear;
} task_list_t;

typedef struct phypg {
  void          *pa;
  int           refcnt;
  struct phypg  *prev;
  struct phypg  *next;
} phypg_t;

typedef struct {
  phypg_t *head;
  phypg_t *rear;
} phypg_list_t;

typedef struct cond_node{
  task_t           *task;
  struct cond_node *prev;
  struct cond_node *next;
} cond_node_t;

typedef struct {
  cond_node_t *head;
  cond_node_t *rear;
} cond_t;

struct semaphore {
  spinlock_t  lk;
  int         value;      //>=0, 代表资源数
  const char  *name;     //debug
  cond_t      cond;
};

struct task {
  cpu_t*            cpu;
  spinlock_t        lk; //lock per task, in order to ganruantee read mutually.
  int               alarm;
  int               pid;
  int               child_ret;
  enum thread_state state;
  const char        *name; //for debugging.
  struct task       *prev;
  struct task       *next; //tasks are arranged in a linked list.
  struct task       *parent;
  Context           *context;
  AddrSpace         as;
  void              *vps[NPAGES]; //allocated virtual pages.
  phypg_t           *pps[NPAGES];
  uint8_t           stack[STACK_SIZE];
};

#endif