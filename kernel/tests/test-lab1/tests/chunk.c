#include "listImpl.h"
#include "thread-sync.h"
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#define HEAPSIZE (1 << 22)
#define HEADSIZE (sizeof(CHUNK))
#define BRANCH 8
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))

extern uintptr_t hh, tt;
spinlock_t locks[BRANCH];

typedef struct ChunkHead {
    size_t size;
    struct ChunkHead *next;
} CHUNK;
CHUNK *head[BRANCH]; 

void pmm_init()
{
    void *space = malloc(HEAPSIZE * BRANCH);
    hh = space, tt = space + HEAPSIZE * BRANCH;
    for (int i = 0; i < BRANCH; i++)
    {
        head[i] = space + HEAPSIZE * i;
        CHUNK *first = head[i] + 1;
        first->size = HEAPSIZE - (HEADSIZE << 1);
        head[i]->next = first;
        first->next = NULL;
    }
}

static inline size_t round_power2(size_t x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

void* check_fit(CHUNK* chunk, size_t size) {
    if (chunk->size >= HEADSIZE + size) {
        chunk->size = size;
        return chunk + 1;
    }
    return NULL;
}

void judge(int k, int line) {
    CHUNK *itr = head[k];
    while (itr->next != NULL) {
        if ((void *)itr->next - (void *)head[k] > (uint64_t)HEAPSIZE) {
            printf("fail:line=%d, address=%p, size=%d\n", line, itr, itr->size);
        }
        assert((void *)itr->next - (void *)head[k] <= (uint64_t)HEAPSIZE);
        itr = itr->next;
    }
    // printf("success:line=%d\n", line);
}

void print(int k) {
    CHUNK *itr = head[k];
    while (itr->next != NULL) {
        printf("itr=%p, itr->next=%p\n", itr, itr->next);
        itr = itr->next;
    }
}

void* check_fit_2(CHUNK *front, CHUNK* chunk, size_t size, int k) {
    // judge(k, 72);
    size_t alignment = round_power2(size);
    void *end = (void *)chunk + HEADSIZE + chunk->size;
    void *start = (void *)chunk;
    void *valid_start = (void *)ROUNDUP((uintptr_t)(start + 2 * HEADSIZE), alignment);
    assert(valid_start >= 2 * HEADSIZE + start);
    size_t old_size = chunk->size;
    if (chunk->size >= HEADSIZE + size && end >= valid_start + size + HEADSIZE)
    {
        assert(chunk->size);
        CHUNK *left = NULL, *right = NULL, *new_chunk;
        // print(k);
        // judge(k, 83);

        CHUNK *nnext = chunk->next;
                // judge(k, 86);

        left = chunk;
                // judge(k, 89);

                // print(k);
        left->size = (size_t)(valid_start - start - 2 * HEADSIZE);
                // printf("left->size = %p, front = %p, front->next = %p, left=%p\n",&(left->size), front, front->next, left);
                // print(k);

                // judge(k, 94);

        // front->next = left;
                // judge(k, 97);


        new_chunk = (CHUNK *)valid_start - 1;
        // judge(k, 101);

        new_chunk->size = size;
                // judge(k, 104);

        right = (CHUNK *)(valid_start + size);
                // judge(k, 108);

        assert(end >= valid_start + size + HEADSIZE);
        right->size = end - valid_start - size - HEADSIZE;
        // print(k);
        // judge(k, 113);
        left->next = right;
        right->next = nnext;
        // print(k);
        // judge(k, 117);
        // printf("left = %p, right = %p\n", left, right);
        // printf("chunk->next=%p\n", nnext);
        // printf("new_chunk=%p, right=%p\n",new_chunk, right);
        // printf("%d, %d\n", left->size + new_chunk->size + right->size + 2 * HEADSIZE, old_size);
        assert(left->size + new_chunk->size + right->size + 2 * HEADSIZE == old_size);
        return valid_start;
    }
    // judge(k, 122);
    return NULL;
}

void merge(int k) {
    // judge(k, 127);
    CHUNK *candi = head[k];
    CHUNK *itr = head[k];
    while (itr != NULL && itr->next != NULL){
        // printf("itr=%p, itr->next=%p, candi->next=%p\n", itr, itr->next, candi->next);
        if ((void *)itr->next == (void *)candi->next + HEADSIZE + candi->next->size)
        {
            candi->next->size += itr->next->size + HEADSIZE;
            itr->next = itr->next->next;
            itr = head[k];
            // judge(k, 137);
            continue;
        }
        else if ((void *)candi->next == (void *)itr->next + HEADSIZE + itr->next->size)
        {
            itr->next->size += candi->next->size + HEADSIZE;
            assert(itr->next->size <= HEAPSIZE);
            candi->next = candi->next->next;
            candi = itr;
            itr = head[k];
            // judge(k, 147);
            continue;
        }
        itr = itr->next;
    }
    return;
}

void* kalloc(size_t size) {
    int k = rand() % BRANCH;
    spin_lock(&locks[k]);
    // judge(k, 158);
    CHUNK *itr = head[k];
    while (itr->next != NULL) {
        size_t old_size = itr->next->size;
        void *valid_start = check_fit_2(itr, itr->next, size, k);
        if (valid_start != NULL) {
            // CHUNK *next_chunk = (CHUNK *)(valid_start + itr->next->size);
            // next_chunk->next = itr->next->next;
            // itr->next = next_chunk;
            // next_chunk->size = old_size - size - HEADSIZE;
            // assert(next_chunk->size <= HEAPSIZE);
            // judge(k, 169);
            spin_unlock(&locks[k]);
            return valid_start;
        }
        itr = itr->next;
    }
    // judge(k, 175);
    spin_unlock(&locks[k]);
    return NULL;
}

void kfree(void* ptr) {
    int k = (ptr - (void*)head[0]) / HEAPSIZE;
    spin_lock(&locks[k]);
        // judge(k, 183);
    CHUNK *p = (CHUNK *)ptr - 1;
    // printf("p = %p, ptr = %p\n", p, ptr);
    // judge(k, 185);
    p->next = head[k]->next;
    // printf("p = %p, &p->next=%p\n",p, &(p->next));
    // judge(k, 187);

    head[k]->next = p;
        // judge(k, 190);

    merge(k);
    // judge(k, 193);
    spin_unlock(&locks[k]);
}