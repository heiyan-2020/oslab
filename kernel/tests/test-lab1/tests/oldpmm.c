#include <common.h>
#include <rand.h>
#include <lock.h>
#include <stdint.h>
#include <stddef.h>
size_t HEAPSIZE;
#define HEADSIZE (sizeof(CHUNK))
#define BRANCH 8
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
spinlock_t locks[BRANCH];

typedef struct ChunkHead {
    size_t size;
    struct ChunkHead *next;
} CHUNK;
CHUNK *head[BRANCH];
CHUNK *avg[BRANCH];

#ifndef TEST
// 框架代码中的 pmm_init (在 AbstractMachine 中运行)
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  //init
  void* base = heap.start;
  HEAPSIZE = (heap.end - heap.start) / BRANCH;
  for (int i = 0; i < BRANCH; i++)
  {
    head[i] = base + HEAPSIZE * i;
    avg[i] = head[i];
    CHUNK *first = head[i] + 1;
    first->size = HEAPSIZE - (HEADSIZE << 1);
    head[i]->next = first;
    first->next = NULL;
  }
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
}
#else
// 测试代码的 pmm_init ()
static void pmm_init() {
  HEAPSIZE = (1 << 29) / BRANCH;
  void *space = malloc(HEAPSIZE * BRANCH);
  for (int i = 0; i < BRANCH; i++)
  {
    head[i] = space + HEAPSIZE * i;
    avg[i] = head[i];
    CHUNK *first = head[i] + 1;
    first->size = HEAPSIZE - (HEADSIZE << 1);
    head[i]->next = first;
    first->next = NULL;
  }
}
#endif
static inline size_t round_power2(size_t x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

void* check_fit_2(CHUNK *front, CHUNK* chunk, size_t size, int k) {
    size_t alignment = round_power2(size);
    void *end = (void *)chunk + HEADSIZE + chunk->size;
    void *start = (void *)chunk;
    void *valid_start = (void *)ROUNDUP((uintptr_t)(start + 2 * HEADSIZE), alignment);
    if (chunk->size >= HEADSIZE + size && end >= valid_start + size + HEADSIZE)
    {
        CHUNK *left = NULL, *right = NULL, *new_chunk;
        CHUNK *nnext = chunk->next;
        left = chunk;
        left->size = (size_t)(valid_start - start - 2 * HEADSIZE); 
        new_chunk = (CHUNK *)valid_start - 1;
        new_chunk->size = size;
        right = (CHUNK *)(valid_start + size);
        right->size = end - valid_start - size - HEADSIZE;
        left->next = right;
        right->next = nnext;
        front->next = right;
        return valid_start;
    }
    return NULL;
}

void merge(int k) {
    CHUNK *candi = head[k];
    CHUNK *itr = head[k];
    while (itr != NULL && itr->next != NULL){
      if ((void *)itr->next == (void *)candi->next + HEADSIZE + candi->next->size)
      {
          candi->next->size += itr->next->size + HEADSIZE;
          itr->next = itr->next->next;
          itr = head[k];
          return;
          // continue;
      }
      else if ((void *)candi->next == (void *)itr->next + HEADSIZE + itr->next->size)
      {
          itr->next->size += candi->next->size + HEADSIZE;
          candi->next = candi->next->next;
          candi = itr;
          itr = head[k];
          return;
          // continue;
      }
      itr = itr->next;
    }
    return;
}

void* kalloc(size_t size) {
  int k = rand() % BRANCH;
  if (size == 0)
  {
    return NULL;
  }
  spin_lock(&locks[k]);
  CHUNK *itr = avg[k];
  void *valid_start = check_fit_2(itr, itr->next, size, k);
  if (valid_start != NULL) {
      spin_unlock(&locks[k]);
      return valid_start;
  }
  //fast path
  itr = head[k];
  //slow path 
  while (itr->next != NULL)
  {
    valid_start = check_fit_2(itr, itr->next, size, k);
    if (valid_start != NULL)
    {
      spin_unlock(&locks[k]);
      return valid_start;
    }
    itr = itr->next;
  }
  spin_unlock(&locks[k]);
  return NULL;
}

void kfree(void* ptr) {
  int k = (ptr - (void *)head[0]) / HEAPSIZE;
  spin_lock(&locks[k]);
  CHUNK *p = (CHUNK *)ptr - 1;
  p->next = head[k]->next;
  head[k]->next = p;
  // merge(k);
  spin_unlock(&locks[k]);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
