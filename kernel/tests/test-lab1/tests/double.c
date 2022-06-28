#include "listImpl.h"
#include <time.h>
#include <stdlib.h>
#include <assert.h>
typedef int spinlock_t;
#define SPIN_INIT() 0

static inline int atomic_xchg(volatile int *addr, int newval) {
  int result;
  asm volatile ("lock xchg %0, %1":
    "+m"(*addr), "=a"(result) : "1"(newval) : "memory");
  return result;
}

void spin_lock(spinlock_t *lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, 1);
    if (value == 0) {
      break;
    }
  }
}
void spin_unlock(spinlock_t *lk) {
  atomic_xchg(lk, 0);
}
#define BRANCH 8
//used locks
spinlock_t locks[BRANCH];
// space
Interval space_pool[BRANCH][INTERVALS_LIMIT];
Interval head[BRANCH];
int q[BRANCH][INTERVALS_LIMIT];
int hh[BRANCH], tt[BRANCH];
uintptr_t L, R, SLICE;

void inter_init() {
    for (int k = 0; k < BRANCH; k++) {
        tt[k] = -1;
        for (int i = 0; i < INTERVALS_LIMIT; i++)
        {
            tt[k] = (tt[k] + 1) % INTERVALS_LIMIT;
            q[k][tt[k]] = i;
        } 
    }
}

void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)HEAP_END - (uintptr_t)HEAP_START);
  //init
  L = (uintptr_t)HEAP_START, R = (uintptr_t)HEAP_END;
  SLICE = (R - L) / BRANCH;
  inter_init();
  printf("Got %ld MiB heap: [%d, %d)\n", pmsize >> 20, HEAP_START, HEAP_END);
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

static  inline uintptr_t check_bound(uintptr_t align, size_t size, uintptr_t left, uintptr_t right) {
    uintptr_t start = ROUNDUP(left, align);
    if (start >= right) {
        return -1;
    }
    return (right - start) >= size ? start : -1;
}

Interval* mini_alloc(uintptr_t l, uintptr_t r, Interval* next, int k) {
    int current = q[k][hh[k]];
    hh[k] = (hh[k] + 1) % INTERVALS_LIMIT;
    space_pool[k][current].l = l, space_pool[k][current].r = r, space_pool[k][current].next = next;
    // spin_unlock(&tlock);
    return &space_pool[k][current];
}

void mini_free(Interval* ptr, int k) {
    //assert(hh >= 0 && tt < INTERVALS_LIMIT - 1);
    // spin_lock(&tlock);
    tt[k] = (tt[k] + 1) % INTERVALS_LIMIT;
    q[k][tt[k]] = ptr->idx;
    // spin_unlock(&tlock);
}

uintptr_t find_interval(size_t size, int left, int right, int k) {
    size_t align = round_power2(size);
    uintptr_t left_bound = left, rtn = -1;
    Interval *itr = &head[k];
    while (itr->next != NULL)
    {
        rtn = check_bound(align, size, left_bound, itr->next->l);
        if (rtn != -1)
        {
            Interval *node = mini_alloc(rtn, rtn + size, itr->next, k);
            itr->next = node;
            return rtn;
        }
        left_bound = itr->next->r;
        itr = itr->next;
    }
    rtn = check_bound(align, size, left_bound, right);
    if (rtn != -1)
    {
        Interval *node = mini_alloc(rtn, rtn + size, NULL, k);
        itr->next = node;
        return rtn;
    }
    return NULL;
}

void find_and_free(uintptr_t l, int k) {
    Interval *itr = &head[k];
    while (itr->next != NULL)
    {
        // printf("dead\n");
        if (itr->next->l == l)
        {
            Interval *tmp = itr->next;
            itr->next = tmp->next;
            mini_free(tmp, k);
            return ;
        }
        itr = itr->next;
    }
    assert(0);
}

void *kalloc(size_t size)
{
    int seed = rand() % BRANCH;
    void *result;
    spin_lock(&locks[seed]);
    uintptr_t left_bound = L + seed * SLICE;
    uintptr_t right_bound = left_bound + SLICE;
    result = (void *)find_interval(size, left_bound, right_bound, seed);
    spin_unlock(&locks[seed]);
    return result;
}

void kfree(void *ptr) {
    uintptr_t p = (uintptr_t) ptr;
    int k = (p - L) / SLICE;
    spin_lock(&locks[k]);
    find_and_free(p, k);
    spin_unlock(&locks[k]);
}
