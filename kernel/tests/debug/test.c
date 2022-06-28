#include "common.h"
#include <stdint.h>
#include <thread.h>
// static void entry(int tid) { pmm->alloc(128); }
// static void goodbye()      { printf("End.\n"); }

#define PRODUCERS 4
#define CONSUMERS 1
#define THREADS 4
#define AMOUNT (1 << 20)
#define SMALL (1 << 5)
#define MEDIUM (1 << 7)
#define BIG (1 << 12)
#define BASIC 0
#define ALLOC_RATIO 50
#define BIG_RATIO 5
static inline size_t round_power2(size_t x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

__thread int thread_id;
void stress_test(int no) {
    unsigned int seed;
    int conti = 0;
    void *ptrs[AMOUNT / THREADS];
    thread_id = no;
    int hh = 0, tt = -1, cnt=0;
    for (int i = 0; i < AMOUNT / THREADS; i++)
    {
        int num = rand_r(&seed) % 100;
        if (num <= ALLOC_RATIO) {
            size_t size;
            if (num <= BIG_RATIO) {
                size = BIG;
            } else {
                size = rand_r(&seed) % MEDIUM;
            }
            size_t pow2 = round_power2(size);
            // printf("before alloc\n");
            void* ptr = pmm->alloc(size);
            // printf("%d\n", conti);
            // printf("alloc:%p:size=%ld\n", ptr, size);
            assert(((uintptr_t)ptr & ((pow2 - 1))) == 0);
            conti++;
            if (ptr == NULL)
            {
                cnt++;
                continue;
            }
            ptrs[++tt] = ptr;
        } else if(hh <= tt){
            // printf("free:%p\n", ptrs[hh]);
            conti--;
            pmm->free(ptrs[hh++]);
            // printf("end\n");
        }
    }
    printf("failed rate=%lf\n", (double)cnt / (double)(AMOUNT / THREADS));
}

int cpu_current() {
    return thread_id - 1;
}

int cpu_count() {
    return THREADS;
}

int main(int argc, char *argv[]) {
    pmm->init();
    for (int i = 0; i < THREADS; i++) {
        create(stress_test);
    }
    join();
}