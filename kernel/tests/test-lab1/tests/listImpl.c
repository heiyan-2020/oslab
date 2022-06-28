#include "listImpl.h"
#include "thread-sync.h"
#include <assert.h>
spinlock_t wlock;
spinlock_t rlock;
spinlock_t alock;
spinlock_t flock;
spinlock_t glock;
int readers;
int allocers;
int freers;
Interval space_pool[INTERVALS_LIMIT];
Interval tm;
Interval *head = &tm;
uintptr_t L, R;
int q[INTERVALS_LIMIT], hh=0, tt=-1; 

void acquire_rlock() {
    spin_lock(&rlock);
    readers++;
    if (readers == 1)
    {
        spin_lock(&wlock);
    }
    spin_unlock(&rlock);
}

void release_rlock() {
    spin_lock(&rlock);
    readers--;
    if (readers == 0) {
        spin_unlock(&wlock);
    }
    spin_unlock(&rlock);
}

void acquire_alock() {
    spin_lock(&alock);
    allocers++;
    if (allocers == 1)
    {
        spin_lock(&flock);
    }
    spin_unlock(&alock);
}

void release_alock() {
    spin_lock(&alock);
    allocers--;
    if (allocers == 0) {
        spin_unlock(&flock);
    }
    spin_unlock(&alock);
}

void acquire_flock() {
    spin_lock(&flock);
    freers++;
    if (freers == 1)
    {
        spin_lock(&glock);
    }
    spin_unlock(&flock);
}

void release_flock() {
    spin_lock(&flock);
    freers--;
    if (freers == 0) {
        spin_unlock(&glock);
    }
    spin_unlock(&flock);
}


void inter_init() {
    for (int i = 0; i < INTERVALS_LIMIT; i++) {
        tt = (tt + 1) % INTERVALS_LIMIT;
        q[tt] = i;
    }
}

#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
//找到2^i>=s
size_t round_power2(size_t x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

uintptr_t check_bound(uintptr_t align, size_t size, uintptr_t left, uintptr_t right) {
    uintptr_t start = ROUNDUP(left, align);
    if (start >= right) {
        return -1;
    }
    return (right - start) >= size ? start : -1;
}

Interval* mini_alloc(uintptr_t l, uintptr_t r, Interval* next) {
    //assert(hh < tt);
    // spin_lock(&tlock);
    int current = q[hh];
    hh = (hh + 1) % INTERVALS_LIMIT;
    space_pool[current].l = l, space_pool[current].r = r, space_pool[current].next = next;
    // spin_unlock(&tlock);
    return &space_pool[current];
}

void mini_free(Interval* ptr) {
    //assert(hh >= 0 && tt < INTERVALS_LIMIT - 1);
    // spin_lock(&tlock);
    tt = (tt + 1) % INTERVALS_LIMIT;
    q[tt] = ptr->idx;
    // spin_unlock(&tlock);
}

void insert(Interval** front, Interval* node) {
    // if (front == NULL) {
    //     return;
    // }
    // int result;
    // asm volatile("lock xchg %0, %1"
    //              : "+m"(*front), "=a"(result)
    //              : "1"(node)
    //              : "memory");
    // spin_lock(&rlock);
     *front = node;
    // spin_unlock(&rlock);
}

uintptr_t write_ope(int l, int r, Interval** front, Interval* old_val) {
    spin_lock(&wlock);
    if (old_val != *front) {
        spin_unlock(&wlock);
        return NULL;
    }
    Interval *node = mini_alloc(l, r, *front);
    *front = node;
    spin_unlock(&wlock);
    return l;
}
/*
@begin:需要进行对齐的区间左端点要求
@size:需要分配的区间大小
@return:如果完成分配，则为区间左端点;否则返回-1
*/
uintptr_t find_interval(size_t size) {
    size_t align = round_power2(size);
    // assert(!(align & (align - 1))); //假设传入的begin已经对齐到2^i了
    uintptr_t left_bound = L, rtn = -1;
    acquire_alock();
    acquire_rlock();
    Interval *itr = head;
    while (itr->next != NULL)
    {
        rtn = check_bound(align, size, left_bound, itr->next->l);
        if (rtn != -1)
        {
            Interval *old_val = itr->next;
            // spin_unlock(&rlock);
            release_rlock();
            // uintptr_t ptr = write_ope(rtn, rtn + size, param);
            //************************************************
            spin_lock(&wlock);
            if (old_val == itr->next) {
                Interval *node = mini_alloc(rtn, rtn + size, itr->next);
                itr->next = node;
                spin_unlock(&wlock);
                release_alock();
                return rtn;
            } else {
                spin_unlock(&wlock);
                acquire_rlock();
                left_bound = itr->next->r;
                itr = itr->next;
                continue;
            }
            //*************************************************
        }
        left_bound = itr->next->r;
        itr = itr->next;
    }
    do {
        rtn = check_bound(align, size, left_bound, R);
        if (rtn != -1)
        {
            //************************************************
            release_rlock();
            spin_lock(&wlock);
            if (itr->next == NULL) {
                Interval *node = mini_alloc(rtn, rtn + size, NULL);
                itr->next = node;
                spin_unlock(&wlock);
                release_alock();
                return rtn;
            } else {
                spin_unlock(&wlock);
                acquire_rlock();
                left_bound = itr->next->r;
                itr = itr->next;
                continue;
            }
            //*************************************************
        }
        release_rlock();
        release_alock();
        return NULL;
    } while (1);
}

void find_and_free(uintptr_t l) {
    spin_lock(&flock);
    Interval *itr = head;
    while (itr->next != NULL) {
        if (itr->next->l == l)
        {
            Interval *tmp = itr->next;
            itr->next = tmp->next;
            mini_free(tmp);
            spin_unlock(&flock);
            return;
        }
        itr = itr->next;
    }
    spin_unlock(&flock);
    return;
}

void *kalloc(size_t size)
{
    // printf("kalloc in\n");
    void *rtn = (void *)find_interval(size);
    // printf("kalloc out\n");
    return rtn;
}

void kfree(void *ptr) {
  // printf("kfree in\n");
  find_and_free((uintptr_t)ptr);
  // printf("kfree out\n");
}

void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)HEAP_END - (uintptr_t)HEAP_START);
  //init
  L = (uintptr_t)HEAP_START, R = (uintptr_t)HEAP_END;
  inter_init();
  printf("Got %ld MiB heap: [%d, %d)\n", pmsize >> 20, HEAP_START, HEAP_END);
}