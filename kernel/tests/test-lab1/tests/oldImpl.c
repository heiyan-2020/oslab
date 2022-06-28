#include "listImpl.h"
#include "thread-sync.h"
spinlock_t wlock;
spinlock_t tlock;
spinlock_t rlock;
int readers;
Interval space_pool[INTERVALS_LIMIT];
Interval tm;
Interval *head = &tm;
uintptr_t L, R;
int q[INTERVALS_LIMIT], hh=0, tt=-1; 

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
    int current = q[hh];
    hh = (hh + 1) % INTERVALS_LIMIT;
    space_pool[current].l = l, space_pool[current].r = r, space_pool[current].next = next;
    return &space_pool[current];
}

void mini_free(Interval* ptr) {
    //assert(hh >= 0 && tt < INTERVALS_LIMIT - 1);
    tt = (tt + 1) % INTERVALS_LIMIT;
    q[tt] = ptr->idx;
}

void insert(Interval* front, Interval* node) {
    if (front == NULL) {
        return;
    }
    front->next = node;
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

    Interval *itr = head;
    while (itr->next != NULL)
    {
        rtn = check_bound(align, size, left_bound, itr->next->l);
        if (rtn != -1)
        {
            Interval *node = mini_alloc(rtn, rtn + size, itr->next);
            insert(itr, node);
            return rtn;
        }
        left_bound = itr->next->r;
        itr = itr->next;
    }
    rtn = check_bound(align, size, left_bound, R);
    if (rtn != -1)
    {
        Interval *node = mini_alloc(rtn, rtn + size, NULL);
        insert(itr, node);
        return rtn;
    }
    return NULL;
}

void find_and_free(uintptr_t l) {
    Interval *itr = head;
    while (itr->next != NULL) {
        // printf("dead\n");
        if (itr->next->l == l)
        {
            Interval *tmp = itr->next;
            insert(itr, tmp->next);
            mini_free(tmp);
            return ;
        }
        itr = itr->next;
    }
        return;
    // assert(0);
}

void *kalloc(size_t size)
{
    // printf("kalloc in\n");
    spin_lock(&rlock);
    // printf("enter kalloc\n");
    void *rtn = (void *)find_interval(size);
    // printf("exit kalloc\n");
    spin_unlock(&rlock);
    // printf("kalloc out\n");
    return rtn;
}

void kfree(void *ptr) {
  // printf("kfree in\n");
    spin_lock(&rlock);
    // printf("enter kfree\n");
    find_and_free((uintptr_t)ptr);
    // printf("exit kfree\n");
    spin_unlock(&rlock);
    // printf("kfree out\n");
}

void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)HEAP_END - (uintptr_t)HEAP_START);
  //init
  L = (uintptr_t)HEAP_START, R = (uintptr_t)HEAP_END;
  inter_init();
  printf("Got %ld MiB heap: [%d, %d)\n", pmsize >> 20, HEAP_START, HEAP_END);
}