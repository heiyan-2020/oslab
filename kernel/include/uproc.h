#include <os.h>
#include <syscall.h>
#include <kmt.h>
/*============================================
                  declaration
=============================================*/
void uproc_init();
int ucreate(task_t *task, char *);
Context* page_fault(Event e, Context *ctx);
Context* syscall(Event e, Context *ctx);

void syscall_kputc(Context *ctx);
void syscall_getpid(Context *ctx);
void syscall_exit(Context *ctx);
void syscall_sleep(Context *ctx);
void syscall_fork(Context *ctx);
void syscall_uptime(Context *ctx);
void syscall_wait(Context *ctx);
void syscall_mmap(Context *ctx);

typedef void (*syscall_hanlder_t)(Context *);