#include "listImpl.h"
#include "thread-sync.h"
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#define BRANCH 8
#define LIMIT (1 << 14)
//used locks
spinlock_t locks[BRANCH];
spinlock_t rlock[BRANCH];
spinlock_t wlock[BRANCH];
spinlock_t alock[BRANCH];
spinlock_t flock[BRANCH];
spinlock_t glock[BRANCH];
int readers[BRANCH];
int allocers[BRANCH];
int freers[BRANCH];
// space
Interval space_pool[BRANCH][INTERVALS_LIMIT];
Interval head[BRANCH];
int len[BRANCH];
int q[BRANCH][INTERVALS_LIMIT];
int hh[BRANCH], tt[BRANCH];
uintptr_t L, R, SLICE;

void acquire_readlock(int k) {
    spin_lock(&rlock[k]);
        readers[k]++;
    if (readers[k] == 1)
    {
        spin_lock(&wlock[k]);
    }
    spin_unlock(&rlock[k]);
}

void release_readlock(int k) {
    spin_lock(&rlock[k]);
    readers[k]--;
    if (readers[k] == 0) {
        spin_unlock(&wlock[k]);
    }
    spin_unlock(&rlock[k]);
}

void acquire_alock(int k) {
    spin_lock(&alock[k]);
    allocers[k]++;
    if (allocers[k] == 1)
    {
        spin_lock(&glock[k]);
    }
    spin_unlock(&alock[k]);
}

void release_alock(int k) {
    spin_lock(&alock[k]);
    allocers[k]--;
    if (allocers[k] == 0) {
        spin_unlock(&glock[k]);
    }
    spin_unlock(&alock[k]);
}

void acquire_flock(int k) {
    spin_lock(&flock[k]);
    freers[k]++;
    if (freers[k] == 1)
    {
        spin_lock(&glock[k]);
    }
    spin_unlock(&flock[k]);
}

void release_flock(int k) {
    spin_lock(&flock[k]);
    freers[k]--;
    if (freers[k] == 0) {
        spin_unlock(&glock[k]);
    }
    spin_unlock(&flock[k]);
}

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

uintptr_t check_bound(uintptr_t align, size_t size, uintptr_t left, uintptr_t right) {
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
            len[k]++;
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
        len[k]++;
        return rtn;
    }
    return NULL;
}

uintptr_t find_fast(size_t size, int left, int right, int k) {
    size_t align = round_power2(size);
    // assert(!(align & (align - 1))); //假设传入的begin已经对齐到2^i了
    uintptr_t left_bound = left, rtn = -1;
  //  acquire_alock(k);
    acquire_readlock(k);
    Interval *itr = &head[k];
    while (itr->next != NULL)
    {
        rtn = check_bound(align, size, left_bound, itr->next->l);
        if (rtn != -1)
        {
            Interval *old_val = itr->next;
            // spin_unlock(&rlock);
            release_readlock(k);
            // uintptr_t ptr = write_ope(rtn, rtn + size, param);
            //************************************************
            spin_lock(&wlock[k]);
            if (old_val == itr->next) {
                Interval *node = mini_alloc(rtn, rtn + size, itr->next, k);
                itr->next = node;
                len[k]++;
                spin_unlock(&wlock[k]);
        //        release_alock(k);
                return rtn;
            } else {
                spin_unlock(&wlock[k]);
                acquire_readlock(k);
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
        rtn = check_bound(align, size, left_bound, right);
        if (rtn != -1)
        {
            //************************************************
            release_readlock(k);
            spin_lock(&wlock[k]);
            if (itr->next == NULL) {
                Interval *node = mini_alloc(rtn, rtn + size, NULL, k);
                itr->next = node;
                len[k]++;
                spin_unlock(&wlock[k]);
       //         release_alock(k);
                return rtn;
            } else {
                spin_unlock(&wlock[k]);
                acquire_readlock(k);
                left_bound = itr->next->r;
                itr = itr->next;
                continue;
            }
            //*************************************************
        }
        release_readlock(k);
     //   release_alock(k);
        return NULL;
    } while (1);
}

void free_fast(uintptr_t l, int k) {
    acquire_flock(k);
    Interval *itr = &head[k];
    while (itr->next != NULL) {
        if (itr->next->l == l)
        {
            Interval *tmp = itr->next;
            itr->next = tmp->next;
            mini_free(tmp, k);
            len[k]--;
            release_flock(k);
            return;
        }
        itr = itr->next;
    }
    release_flock(k);
    return;
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
            len[k]--;
            return;
        }
        itr = itr->next;
    }
    assert(0);
}

void *kalloc(size_t size)
{
    int seed = rand() % BRANCH;
    void *result;
    uintptr_t left_bound = L + seed * SLICE;
    uintptr_t right_bound = left_bound + SLICE;
    if (len[seed] > LIMIT)
    {
        result = (void *)find_fast(size, left_bound, right_bound, seed);
    }
    else
    {
        spin_lock(&locks[seed]);
        result = (void *)find_interval(size, left_bound, right_bound, seed);
        spin_unlock(&locks[seed]);
    }
    return result;
}

void kfree(void *ptr) {
    uintptr_t p = (uintptr_t) ptr;
    int k = (p - L) / SLICE;
    // if (len[k] > LIMIT) {
    //     free_fast(p, k);
    // } else {
        spin_lock(&locks[k]);
        find_and_free(p, k);
        spin_unlock(&locks[k]);
 //   }
}
