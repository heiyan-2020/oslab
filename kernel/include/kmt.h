#include <os.h>
#include <task.h>
#include <log.h>
#include <cond.h>

void kmt_init();
int kcreate(task_t *task, const char *name, void (*entry)(void *arg), void *arg);
void teardown(task_t *task);
void spin_init(spinlock_t *lk, const char *name);
void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);
void sem_init(sem_t *sem, const char *name, int value);
void sem_wait(sem_t *sem);
void sem_signal(sem_t *sem);
Context *schedule(Event ev, Context *context);
void push_off();
void pop_off();
int holding(spinlock_t *lk);
cpu_t *mycpu();
task_t *mytask();
int check_race(task_t *);
void sleep(cond_t *cond, spinlock_t *);
void wakeup(cond_t *cond);
void initpid();
int allocpid();
void freepid(int pid);
void exit();

void handler_DEFAULT(task_t *task, Context *);
void handler_SLEEPY(task_t *task, Context *);
void handler_DEAD(task_t *task, Context *);
void handler_ERROR(task_t *task, Context *);

extern task_list_t *tlist_;
extern spinlock_t trap_lk;

typedef void (*state_handler_t)(task_t *task, Context *);