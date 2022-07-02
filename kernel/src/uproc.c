#include <uproc.h>
#include <page.h>
#include <user.h>

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
    virtpg_t *virtual_page = virt_node_make(va, page);
    virt_list_insert(&task->vps, virtual_page);
    assert((uintptr_t)va == ROUNDDOWN(va,task->as.pgsize));
    assert((uintptr_t)pa == ROUNDDOWN(pa,task->as.pgsize));
    map(&task->as, va, pa, MMAP_READ | MMAP_WRITE);
}

Context* page_fault(Event e, Context *ctx) {
    AddrSpace *as = &mytask()->as;
    void *va = (void *)(e.ref & ~(as->pgsize - 1));
    assert((uintptr_t)va == ROUNDDOWN(va,as->pgsize));
    virtpg_t *ori_vpg = virt_list_find(&mytask()->vps, va);
    phypg_t *page = NULL;
    if (ori_vpg == NULL) {
        if (e.cause == 1) {
            //read a new page.
            page = alloc_page(as->pgsize);
            if (va == as->area.start) {
                assert(_init_len < as->pgsize);
                memcpy(page->pa, _init, _init_len);
            }
            MEMLOG("read new page of %p\n", e.ref);
            page_map(mytask(), va, page);
        } else {
            //write a new page.
            page = alloc_page(as->pgsize);
            MEMLOG("Write new page of %p\n", va);
            page_map(mytask(), va, page);
        }
    } else {
        if (e.cause == 1) {
            //read a mmaped page.
            MEMLOG("Read mmaped page of %p\n", va);
            ori_vpg->page->pa = pmm->alloc(as->pgsize);
            map(as, va, ori_vpg->page->pa, ori_vpg->page->prot);
        } else {
            if (ori_vpg->page->pa == NULL) {
                //write a mmaped page.
                MEMLOG("Write mmaped page of %p\n", va);
                if (ori_vpg->page->flags == MAP_PRIVATE) {
                    phypg_t *ppage = pmm->alloc(sizeof(phypg_t));
                    ppage->flags = ori_vpg->page->flags;
                    ppage->prot = ori_vpg->page->prot;
                    ppage->pa = pmm->alloc(as->pgsize);
                    ppage->refcnt = 1; 
                    ori_vpg->page->refcnt--;
                    ori_vpg->page = ppage;
                } else {
                    assert(ori_vpg->page->flags == MAP_SHARED);
                    ori_vpg->page->pa = pmm->alloc(as->pgsize);
                }
                map(as, va, ori_vpg->page->pa, ori_vpg->page->prot);
            } else {
                //write an existed page without protect.
                phypg_t *ori_ppg = ori_vpg->page;
                MEMLOG("Clear map prot\n");
                assert((uintptr_t)ori_ppg->pa == ROUNDDOWN(ori_ppg->pa,as->pgsize));
                map(as, va, ori_ppg->pa, MMAP_NONE);
                if (ori_vpg->page->flags) {
                    map(as, va, ori_ppg->pa, MMAP_READ | MMAP_WRITE);
                } else {
                    if (ori_ppg->refcnt == 1) {
                        MEMLOG("Last ref of %p on %p\n", ori_ppg->pa, va);
                        assert((uintptr_t)ori_ppg->pa == ROUNDDOWN(ori_ppg->pa,as->pgsize));
                        map(as, va, ori_ppg->pa, MMAP_READ | MMAP_WRITE);
                    } else {
                        ori_ppg->refcnt--;
                        page = alloc_page(as->pgsize);
                        memcpy(page->pa, ori_ppg->pa, as->pgsize);
                        ori_vpg->page = page;
                        MEMLOG("Copy on Write of %p, new physical page = %p\n", va, page->pa);
                        assert((uintptr_t)page->pa == ROUNDDOWN(page->pa,as->pgsize));
                        map(as, va, page->pa, MMAP_READ | MMAP_WRITE);
                    }
                }
            }
        }
    }
    return NULL;
}

Context* syscall(Event e, Context *ctx) {
    // iset(true);
    #ifdef STRACE
    printf("[strace]%s %d\n", syscall_name[ctx->GPRx], mytask()->pid);
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

    virtpg_t *parent_itr = mytask()->vps.head->next;
    while (parent_itr != mytask()->vps.rear) {
        void *va = parent_itr->va;
        phypg_t *page = parent_itr->page;
        if (page->pa != NULL) {
            assert((uintptr_t)page->pa == ROUNDDOWN(page->pa,mytask()->as.pgsize));
            map(&child->as, va, page->pa, MMAP_READ);
            map(&mytask()->as, va, page->pa, MMAP_NONE);
            map(&mytask()->as, va, page->pa, MMAP_READ); //mark as non-writable.
        }

        virtpg_t *virt_page = virt_node_make(va, page);
        virt_list_insert(&child->vps, virt_page);
        parent_itr = parent_itr->next;
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
    while (itr != tlist_->rear) {
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
        if (vaddr != NULL) {
            void *va_start = (void *)ROUNDDOWN(vaddr, cur->as.pgsize);
            int offset = vaddr - va_start;
            virtpg_t *vpage = virt_list_find(&cur->vps, va_start);
            assert(vpage != NULL);
            int *paddr = (int *)(vpage->page->pa + offset);
            *paddr = cur->child_ret;
        }
        cur->child_ret = MAGIC_NUM;
        ctx->GPRx = 0;
    }
}

void syscall_exit(Context *ctx) {
    ctx->GPRx = ctx->GPR1; //返回值wait会用到
    mytask()->parent->child_ret = ctx->GPR1;
    exit();
}

#define RIGHT(va, len) (va + len)
void *find_avaliable(virtpg_list_t *list, void *va, int len) {
    int pgsize = mytask()->as.pgsize;
    void *left = (void *)ROUNDUP((intptr_t)va, pgsize);
    len = (int)ROUNDUP((intptr_t)len, pgsize);
    if (left == NULL) {
        left = mytask()->as.area.start;
    }

    virtpg_t *itr = list->head->next;
    while (itr != list->rear) {
        if (RIGHT(itr->va, pgsize) > left && itr->va < RIGHT(left, len)) {
            left = RIGHT(itr->va, pgsize);
        } else {
            return left;
        }
        itr = itr->next;
    }
    assert(left >= mytask()->as.area.start);
    if (left < mytask()->as.area.end && RIGHT(left, len) <= mytask()->as.area.end) {
        return left;
    } else {
        return NULL;
    }
}

void syscall_mmap(Context *ctx) {
    void *addr = (void*)ctx->GPR1;
    int len = (int)ctx->GPR2;
    int prot = (int)ctx->GPR3;
    int flags = (int)ctx->GPR4;
    // assert(flags == MAP_PRIVATE || flags == MAP_UNMAP);

    if (flags == MAP_PRIVATE || flags == MAP_SHARED) {
        len = (int)ROUNDUP((intptr_t)len, mytask()->as.pgsize);
        void *ava_vaddr = find_avaliable(&mytask()->vps, addr, len);
        if (ava_vaddr == NULL) {
            ctx->GPRx = -1;
            return;
        }
        for (void *itr = ava_vaddr; itr < RIGHT(ava_vaddr, len); itr += mytask()->as.pgsize) {
            phypg_t *ppage = pmm->alloc(sizeof(phypg_t));
            ppage->flags = flags;
            ppage->prot = prot >> 1;//user.h中定义的权限和map时不一致， 这里转换为map中的规范
            ppage->pa = NULL;
            ppage->refcnt = 1; //zero indicates that this physical page is not avaliable yet.
            virtpg_t *vpage = virt_node_make(itr, ppage);
            virt_list_insert(&mytask()->vps, vpage);
        }
        ctx->GPRx = (uint64_t)ava_vaddr;
    } else if (flags == MAP_UNMAP) {
        assert(addr == (void *)ROUNDDOWN(addr, mytask()->as.pgsize));//addr must be multiple of pgsize.
        len = (int)ROUNDUP((intptr_t)len, mytask()->as.pgsize);//align to pgsize.
        for (void *itr = addr; itr < RIGHT(addr, len); itr += mytask()->as.pgsize) {
            if (virt_list_find(&mytask()->vps, itr) == NULL) {
                ctx->GPRx = -1;
                return;
            }
        }
        ctx->GPRx = 0;
        for (void *itr = addr; itr < RIGHT(addr, len); itr += mytask()->as.pgsize) {
            virt_list_remove(&mytask()->vps, virt_list_find(&mytask()->vps, itr));
        }
    }
}
