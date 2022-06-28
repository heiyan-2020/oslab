#include <listImpl.h>
#include <common.h>
#include <lock.h>
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
inline size_t round(size_t x) {
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
/*
@begin:需要进行对齐的区间左端点要求
@size:需要分配的区间大小
@return:如果完成分配，则为区间左端点;否则返回-1
*/
uintptr_t find_interval(size_t size) {
    size_t align = round(size);
    // assert(!(align & (align - 1))); //假设传入的begin已经对齐到2^i了
    Interval *itr = head;
    uintptr_t left_bound = L, rtn = -1;
    while (itr->next != NULL)
    {
        rtn = check_bound(align, size, left_bound, itr->next->l);
        if (rtn != -1)
        {
            Interval *node = mini_alloc(rtn, rtn + size, itr->next);
            itr->next = node;
            return rtn;
        }
        left_bound = itr->next->r;
        itr = itr->next;
    }
    rtn = check_bound(align, size, left_bound, R);
    if (rtn != -1)
    {
        Interval *node = mini_alloc(rtn, rtn + size, itr->next);
        itr->next = node;
        return rtn;
    }
    return -1;
}

void find_and_free(uintptr_t l) {
    Interval *itr = head;
    while (itr->next != NULL) {
        if (itr->next->l == l) {
            Interval *tmp = itr->next;
            itr->next = tmp->next;
            mini_free(tmp);
            return ;
        }
        itr = itr->next;
    }
    return;
    // assert(0);
}