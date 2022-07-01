#include <uproc.h>
#include <page.h>

#include "initcode.inc"
/*============================================
                  interfaces
=============================================*/
MODULE_DEF(uproc) = {
    .init = uproc_init
};
#define ROUNDDOWN(a, sz)    ((((uintptr_t)a)) & ~((sz) - 1))
/*============================================
              global variables
=============================================*/
phypg_list_t *page_list;
extern spinlock_t schedule_lk;
static syscall_hanlder_t syscall_table[] = {
    [SYS_kputc] = syscall_kputc,
    [SYS_getpid] = syscall_getpid,
    [SYS_sleep] = syscall_sleep,
    [SYS_uptime] = syscall_uptime,
    [SYS_fork] = syscall_fork,
    [SYS_wait] = syscall_wait,
    [SYS_exit] = syscall_exit,
    [SYS_mmap] = syscall_mmap
};

#ifdef STRACE
static char* syscall_name[] = {
    [SYS_kputc] = "kputc",
    [SYS_getpid] = "getpid",
    [SYS_sleep] = "sleep",
    [SYS_uptime] = "uptime",
    [SYS_fork] = "fork",
    [SYS_wait] = "wait",
    [SYS_exit] = "exit",
    [SYS_mmap] = "mmap",
};
#endif
/*============================================
              implementations
=============================================*/
void uproc_init() {
    vme_init((void *(*)(int))pmm->alloc, pmm->free);

    os->on_irq(100, EVENT_PAGEFAULT, page_fault); //注册页故障处理程序
    os->on_irq(101, EVENT_SYSCALL, syscall); //注册页故障处理程序

    // page_list_make(page_list);

    MEMLOG("");
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

    // printf("map: %p -> %p\n", va, pa);

    for (int i = 0; i < NPAGES; i++) {
        if (task->vps[i] == NULL) {
            task->vps[i] = va;
            task->pps[i] = page;
            break;
        }
    }
    assert((uintptr_t)va == ROUNDDOWN(va,task->as.pgsize));
    assert((uintptr_t)pa == ROUNDDOWN(pa,task->as.pgsize));
    map(&task->as, va, pa, MMAP_READ | MMAP_WRITE);
}

Context* page_fault(Event e, Context *ctx) {
    AddrSpace *as = &mytask()->as;
    void *va = (void *)(e.ref & ~(as->pgsize - 1));
    assert((uintptr_t)va == ROUNDDOWN(va,as->pgsize));
    phypg_t *page = NULL;
    if (e.cause == 1) {
        //read a new page.
        page = alloc_page(page_list, as->pgsize);
        if (va == as->area.start) {
            memcpy(page->pa, _init, _init_len);
        }
        MEMLOG("read new page of %p\n", e.ref);
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
            MEMLOG("Clear map prot\n");
            assert((uintptr_t)ori_page->pa == ROUNDDOWN(ori_page->pa,as->pgsize));
            map(as, va, ori_page->pa, MMAP_NONE);

            if (ori_page->refcnt == 1) {
                MEMLOG("Last ref of %p on %p\n", ori_page->pa, va);
                assert((uintptr_t)ori_page->pa == ROUNDDOWN(ori_page->pa,as->pgsize));
                map(as, va, ori_page->pa, MMAP_READ | MMAP_WRITE);
            } else {
                ori_page->refcnt--;
                page = alloc_page(page_list, as->pgsize);
                memcpy(page->pa, ori_page->pa, as->pgsize);
                mytask()->pps[num] = page;
                MEMLOG("Copy on Write of %p, new physical page = %p\n", va, page->pa);
                assert((uintptr_t)page->pa == ROUNDDOWN(page->pa,as->pgsize));
                map(as, va, page->pa, MMAP_READ | MMAP_WRITE);
            }
        } else {
            //write a new page.
            page = alloc_page(page_list, as->pgsize);
            MEMLOG("Write new page of %p\n", va);
            page_map(mytask(), va, page);
        }
    }
    return NULL;
}

Context* syscall(Event e, Context *ctx) {
    // iset(true);
    #ifdef STRACE
    printf("[strace]%s\n", syscall_name[ctx->GPRx]);
    #endif
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
    assert(!ienabled());
    task_t *child = pmm->alloc(sizeof (task_t));
    char *name = "_child";
    char *child_name = pmm->alloc(strlen(mytask()->name) + strlen(name) + 4);
    strcpy(child_name, mytask()->name);
    strcat(child_name, name);

    ucreate(child, child_name);
    uintptr_t rsp0 = child->context->rsp0;
    void *cr3 = child->context->cr3;

    *child->context = *ctx;
    child->context->rsp0 = rsp0;
    child->context->cr3 = cr3;
    child->context->GPRx = 0;
    child->parent = mytask();

    for (int i = 0; i < NPAGES; i++) {
        void *va = mytask()->vps[i];
        if (va == NULL) continue;

        phypg_t *page = mytask()->pps[i];
        // phypg_t* page = alloc_page(page_list, mytask()->as.pgsize);
        // memcpy(page->pa, mytask()->pps[i]->pa, mytask()->as.pgsize);
        assert((uintptr_t)page->pa == ROUNDDOWN(page->pa,mytask()->as.pgsize));
        map(&child->as, va, page->pa, MMAP_READ);
        map(&mytask()->as, va, page->pa, MMAP_NONE);
        map(&mytask()->as, va, page->pa, MMAP_READ); //mark as non-writable.
        child->vps[i] = va;
        child->pps[i] = page;
        page->refcnt++;
    }
    assert(child->context->GPRx == 0);
    ctx->GPRx = child->pid;
}

void syscall_wait(Context *ctx) {
    kmt->spin_lock(&schedule_lk);
    bool has_children = false;
    task_t *cur = mytask();
    task_t *itr = tlist_->head->next;
    while (itr != tlist_->head) {
        if (itr->parent == cur) {
            has_children = true;
            break;
        }
        itr = itr->next;
    }
    kmt->spin_unlock(&schedule_lk);
    if (!has_children) {
        ctx->GPRx = -1;
    } else {
        while (cur->child_ret == MAGIC_NUM) {
            yield();
        }
        void *vaddr = (void *)ctx->GPR1;
        void *va_start = (void *)ROUNDDOWN(vaddr, cur->as.pgsize);
        int offset = vaddr - va_start;
        for (int i = 0; i < NPAGES; i++) {
            if (cur->vps[i] == va_start) {
                void *pa_start = cur->pps[i]->pa;
                *(int *)(pa_start + offset) = cur->child_ret;
                cur->child_ret = MAGIC_NUM;
                ctx->GPRx = 0;
                return;
            }
        }
        assert(0);
    }
}

void syscall_exit(Context *ctx) {
    ctx->GPRx = ctx->GPR1; //返回值wait会用到
    mytask()->parent->child_ret = ctx->GPR1;
    exit();
}

void syscall_mmap(Context *ctx) {
    void *addr = (void*)ctx->GPR1;
    int len = (int)ctx->GPR2;
    // int prot = (int)ctx->GPR3;
    int flags = (int)ctx->GPR4;

    void *addr_bound = (void *)ROUNDUP((intptr_t)addr, mytask()->as.pgsize);
    len = (int)ROUNDUP((intptr_t)len, mytask()->as.pgsize);
    while (addr_bound + len < mytask()->as.area.end) {
        bool succ = true;
        for (int i = 0; i < NPAGES; i++) {
            if (mytask()->vps[i] >= addr_bound && mytask()->vps[i] + mytask()->as.pgsize < addr_bound + len) {
                addr_bound = mytask()->vps[i] + mytask()->as.pgsize;
                succ = false;
                break;
            }
        }
        if (succ) {
            assert(flags = MAP_PRIVATE);
            void *end = addr_bound + len;
            ctx->GPRx = (uint64_t)addr_bound;
            printf("addr = %p\n", addr_bound);
            for (int i = 0; i < NPAGES; i++) {
                if (mytask()->vps[i] == NULL) {
                    phypg_t *page = pmm->alloc(sizeof(phypg_t));
                    page->flags = flags;
                    mytask()->vps[i] = addr_bound;
                    mytask()->pps[i] = page;
                    addr_bound += mytask()->as.pgsize;
                    if (addr_bound == end) {
                        break;
                    }
                }
            }
            break;
        }
    }
    ctx->GPRx = -1;
}
