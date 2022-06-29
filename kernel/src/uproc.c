#include <uproc.h>
#include <page.h>

#include "initcode.inc"
/*============================================
                  interfaces
=============================================*/
MODULE_DEF(uproc) = {
    .init = uproc_init
};

/*============================================
              global variables
=============================================*/
phypg_list_t *page_list;

static syscall_hanlder_t syscall_table[] = {
    [SYS_kputc] = syscall_kputc,
    [SYS_getpid] = syscall_getpid,
    [SYS_sleep] = syscall_sleep,
    [SYS_uptime] = syscall_uptime,
    [SYS_fork] = syscall_fork,
    [SYS_wait] = syscall_wait,
    [SYS_exit] = syscall_exit,
};
/*============================================
              implementations
=============================================*/
void uproc_init() {
    vme_init((void *(*)(int))pmm->alloc, pmm->free);

    os->on_irq(100, EVENT_PAGEFAULT, page_fault); //注册页故障处理程序
    os->on_irq(101, EVENT_SYSCALL, syscall); //注册页故障处理程序

    page_list_make(page_list);

    LOG("");
}

int ucreate(task_t *task, char *name) {
    protect(&task->as);
    Context *ctx = ucontext(&task->as, (Area) {&(task->stack), task + 1}, task->as.area.start);
    task_init(task, ctx, name);

    return 0;
}

void page_map(task_t *task, void *va, phypg_t *page) {
    void *pa = page->pa;
    assert(va != NULL);

    printf("map: %p -> %p\n", va, pa);

    for (int i = 0; i < NPAGES; i++) {
        if (task->vps[i] == NULL) {
            task->vps[i] = va;
            task->pps[i] = page;
            break;
        }
    }

    map(&task->as, va, pa, MMAP_READ | MMAP_WRITE);
}

Context* page_fault(Event e, Context *ctx) {
    AddrSpace *as = &mytask()->as;
    void *va = (void *)(e.ref & ~(as->pgsize - 1));
    phypg_t *page = NULL;
    if (e.cause == 1) {
        //read a new page.
        page = alloc_page(page_list, as->pgsize);
        if (va == as->area.start) {
            memcpy(page->pa, _init, _init_len);
        }
        printf("read new page of %p\n", va);
        page_map(mytask(), va, page);
    } else {
        int num = -1;
        for (int i = 0; i < NPAGES; i++) {
            if (mytask()->vps[i] == va) {
                num = i;
                break;
            }
        }
        if (num != -1) {
            //write an existed page without protect.
            phypg_t *ori_page = mytask()->pps[num];
            printf("Clear map prot\n");
            map(as, va, ori_page->pa, MMAP_NONE);

            if (ori_page->refcnt == 1) {
                printf("Last ref of %p\n", ori_page->pa);
                map(as, va, ori_page->pa, MMAP_READ | MMAP_WRITE);
            } else {
                ori_page->refcnt--;
                page = alloc_page(page_list, as->pgsize);
                memcpy(page->pa, ori_page->pa, as->pgsize);
                mytask()->pps[num] = page;
                printf("Copy on Write of %p, new physical page = %p\n", va, page->pa);
                map(as, va, page->pa, MMAP_READ | MMAP_WRITE);
            }
        } else {
            //write a new page.
            page = alloc_page(page_list, as->pgsize);
            printf("Write new page of %p\n", va);
            page_map(mytask(), va, page);
        }
    }
    assert(page != NULL);
    return NULL;
}

Context* syscall(Event e, Context *ctx) {
    // iset(true);
    syscall_table[ctx->GPRx](ctx);
    // iset(false);
    return NULL;
}

void syscall_kputc(Context *ctx) {
    // printf("%s\n", mytask()->name);
    putch(ctx->GPR1);
}

void syscall_getpid(Context *ctx) {
    ctx->GPRx = mytask()->pid;
}

void syscall_sleep(Context *ctx) {
    uint64_t wakeup = io_read(AM_TIMER_UPTIME).us + (ctx->GPR1 * 1000000L);
    while (io_read(AM_TIMER_UPTIME).us < wakeup) {
        yield();
    }
}

void syscall_uptime(Context *ctx) {
    uint64_t uptime = io_read(AM_TIMER_UPTIME).us / 1000;
    ctx->GPRx = uptime;
}

void syscall_fork(Context *ctx) {
    printf("fork\n");
    task_t *child = pmm->alloc(sizeof (task_t));
    char *name = "_child";
    char *child_name = pmm->alloc(strlen(child->name) + strlen(name) + 4);
    strcpy(child_name, mytask()->name);
    strcat(child_name, name);
    // printf("parent->name = %s, child->name = %s\n", mytask()->name, child_name);

    ucreate(child, child_name);
    uintptr_t rsp0 = child->context->rsp0;
    void *cr3 = child->context->cr3;

    *child->context = *ctx;
    child->context->rsp0 = rsp0;
    child->context->cr3 = cr3;
    child->context->GPRx = 0;

    for (int i = 0; i < NPAGES; i++) {
        void *va = mytask()->vps[i];
        if (va == NULL) continue;

        void *pa = mytask()->pps[i]->pa;

        map(&child->as, va, pa, MMAP_READ);
        map(&mytask()->as, va, pa, MMAP_NONE);
        map(&mytask()->as, va, pa, MMAP_READ); //mark as non-writable.
        child->vps[i] = va;
        child->pps[i] = pa;
        mytask()->pps[i]->refcnt++;
        
    }
    assert(child->context->GPRx == 0);
    ctx->GPRx = 1;
}

void syscall_wait(Context *ctx) {
    task_t *itr;
    for (itr = mytask()->children->head->next; itr != mytask()->children->rear; itr = itr->next) {

    }
}

void syscall_exit(Context *ctx) {
    mytask()->state = DEAD;
    ctx->GPRx = ctx->GPR1; //返回值wait会用到
    
    exit();
}
