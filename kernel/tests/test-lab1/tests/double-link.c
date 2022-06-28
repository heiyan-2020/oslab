#include <common.h>
#include <rand.h>
#include <lock.h>
#include <stdint.h>
#include <stddef.h>
size_t HEAPSIZE;
#define HEADSIZE (sizeof(CHUNK))
#define WORDSIZE (sizeof(size_t))
#define LOWER_BOUND (HEADSIZE + WORDSIZE)
#define BRANCH 8
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
#define VALID(p, k) (p != BOUNDARY(head[k]) && p != NULL)

#define SETSIZE(p, s) (p = s << 2) // p is size_t;
#define GETSIZE(p) (p >> 2)
#define SET_STATE(p, s) (p = (p & ~3) | s)
#define GET_STATE(p) (p & 0x3)
#define STATE_ALLOC 0
#define STATE_FREE 1
#define STATE_GAP 2

#define BOUNDARY(p) (void*)((void*)p + (GETSIZE(p->size)) + HEADSIZE) //p is CHUNK*  
#define BOUND_ALLOC(p) ((void*)p + (GETSIZE(*((size_t*)p - 1)))) //p is size_t* or void*

spinlock_t locks[BRANCH];

typedef struct ChunkHead {
    size_t size;// 高位为size, 最低1位为标志位 1代表free 0代表allocated
    struct ChunkHead *prev;
    struct ChunkHead *next;
} CHUNK;
CHUNK *head[BRANCH];
CHUNK *avg[BRANCH];

static inline void PUT(CHUNK* chunk, int k) {
  chunk->next = head[k]->next;
  head[k]->next->prev = chunk;
  head[k]->next = chunk;
  chunk->prev = head[k];
}

static inline void update_size(CHUNK* chunk, size_t size, int state) {
  assert(chunk >= head[0]);
    SETSIZE(chunk->size, size);
  SET_STATE(chunk->size, state);
  size_t *boundary_sizePtr = (size_t*)BOUNDARY(chunk);
  *boundary_sizePtr = size << 2;
  // SETSIZE(*boundary_sizePtr, size);
  SET_STATE(*boundary_sizePtr, state);
}

static inline CHUNK* getFromBound(size_t* bound) {
  assert(GET_STATE(*bound) == STATE_FREE);
  void *start = (void *)bound - GETSIZE(*bound) - HEADSIZE;
  return (CHUNK *)start;
}

#ifndef TEST
// 框架代码中的 pmm_init (在 AbstractMachine 中运行)
#define assert(s) //##s
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
static void pmm_init() {
  HEAPSIZE = (1 << 29) / BRANCH;
  void *space = malloc(HEAPSIZE * BRANCH);
  space = (void *)ROUNDUP(space, HEADSIZE);
  for (int i = 0; i < BRANCH; i++)
  {
    head[i] = space + HEAPSIZE * i;
    CHUNK *first = (void*)head[i] + (HEADSIZE << 1);
    head[i]->next = first;
    head[i]->prev = first;
    first->prev = head[i];
    first->next = head[i];
    update_size(head[i], HEADSIZE - WORDSIZE, STATE_ALLOC);
    update_size(first, HEAPSIZE - (HEADSIZE * 3) - WORDSIZE, STATE_FREE);
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

// void* alloc_split(CHUNK* chunk, size_t size, int k) {
//   assert(size % WORDSIZE == 0); //size对齐到WORDSIZE
//   size_t alignment = round_power2(size);
//   void *align_start = ROUNDUP((void *)(chunk) + WORDSIZE, alignment);

//   if (BOUNDARY(chunk) - align_start >= size) {
//     void *new_chunkHead = align_start - WORDSIZE;
//     size_t *hh = (size_t *)new_chunkHead;
//     SETSIZE(*hh, size);
//     SET_STATE(*hh, STATE_ALLOC);
//     size_t *tt = (size_t *)(align_start + size);
//     SETSIZE(*tt, size);
//     SET_STATE(*tt, STATE_ALLOC);

//     size_t front_gap = new_chunkHead - (void *)chunk;
//     size_t back_gap = BOUNDARY(chunk) - align_start;
//     assert(front_gap % WORDSIZE == 0);
//     assert(back_gap % WORDSIZE == 0);
//     CHUNK *left = chunk->prev, *right = chunk->next;

//     if (back_gap >= LOWER_BOUND) {
//       //后端创建free chunk. 需要
//       right = (chunk *)(align_start + size + WORDSIZE);
//       update_size(right, back_gap - LOWER_BOUND);
//       right->next = chunk->next;
//     }
//     else
//     {
//       size_t template;
//       SETSIZE(template, back_gap);
//       SET_STATE(template, STATE_GAP);
//       size_t *p = (size_t *)(align_start + size + WORDSIZE);
//       for (int i = 0; i < back_gap; i += WORDSIZE) {
//         *p++ = template;
//       }
//     }

//     if (front_gap >= LOWER_BOUND) {
//       //前端保留free chunk. 只需要改变next指针、boundary size、size
//       left = chunk;
//       update_size(left, front_gap - LOWER_BOUND);
//     }
//     else
//     {
//       size_t template;
//       SETSIZE(template, front_gap);
//       SET_STATE(template, STATE_GAP);
//       size_t *p = (size_t *)chunk;
//       for (int i = 0; i < front_gap; i += WORDSIZE) {
//         *p++ = template;
//       }
//     }
//     right->prev = left;
//     left->next = right;
//     return align_start;
//   }
//   return NULL;
// }

void* alloc_split(CHUNK* chunk, size_t size) {
  assert(chunk->next >= head[0]);
  size = size < (HEADSIZE - WORDSIZE) ? HEADSIZE - WORDSIZE : size;
  size += WORDSIZE - (size % WORDSIZE);
  assert(size % WORDSIZE == 0);
  void *start = (void *)chunk + WORDSIZE;
  assert(BOUNDARY(chunk) > start);
  size_t back_gap = BOUNDARY(chunk) - start;
  if (back_gap >= size)
  {
    if (back_gap < LOWER_BOUND + size) {
      chunk->prev->next = chunk->next;
      chunk->next->prev = chunk->prev;
      SETSIZE(chunk->size, back_gap);
      SET_STATE(chunk->size, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, back_gap);
      SET_STATE(*tt, STATE_ALLOC);
    }
    else
    {
      CHUNK *new_chunk = (CHUNK *)(start + size + WORDSIZE);
      new_chunk->prev = chunk->prev;
      new_chunk->next = chunk->next;
      chunk->prev->next = new_chunk;
      chunk->next->prev = new_chunk;
      assert(back_gap >= size + LOWER_BOUND);
      update_size(new_chunk, back_gap - size - LOWER_BOUND, STATE_FREE);

      SETSIZE(chunk->size, size);
      SET_STATE(chunk->size, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, size);
      SET_STATE(*tt, STATE_ALLOC);
    }
    // printf("chunk=%p, size=0x%lx\n", chunk, size);
    return start;
  }
  return NULL;
}

void coalescing(size_t *freeptr, int k) {
  int prev_state = GET_STATE(*(freeptr - 2));
  // printf("freeptr=%p, *=0x%lx\n", freeptr - 1, *(freeptr - 1));
  int next_state = GET_STATE(*(size_t *)(BOUND_ALLOC(freeptr)));
  size_t size = GETSIZE(*(freeptr - 1));
  CHUNK *chunk = (CHUNK *)(freeptr - 1);
  if (prev_state == STATE_ALLOC && next_state == STATE_ALLOC)
  {
    update_size(chunk, size - (HEADSIZE - WORDSIZE), STATE_FREE);
    PUT(chunk, k);
  } else if (prev_state == STATE_FREE && next_state == STATE_ALLOC) {
    CHUNK *prev = getFromBound(freeptr - 2);
    prev->prev->next = prev->next;
    prev->next->prev = prev->prev;
    update_size(prev, GETSIZE(prev->size) + size + 2 * WORDSIZE, STATE_FREE);
    PUT(prev, k);
  } else if (prev_state == STATE_ALLOC && next_state == STATE_FREE) {
    CHUNK *next = (CHUNK*)((size_t *)BOUND_ALLOC(freeptr) + 1);
    next->prev->next = next->next;
    next->next->prev = next->prev;
    CHUNK *new_head = (CHUNK *)(freeptr - 1);
    update_size(new_head, GETSIZE(next->size) + size + 2 * WORDSIZE, STATE_FREE);
    PUT(new_head, k);
  } else if (prev_state == STATE_FREE && next_state == STATE_FREE) {
    CHUNK *prev = getFromBound(freeptr - 2);
    CHUNK *next = (CHUNK*)((size_t *)BOUND_ALLOC(freeptr) + 1);
    prev->prev->next = prev->next;
    prev->next->prev = prev->prev;
    next->prev->next = next->next;
    next->next->prev = next->prev;
    update_size(prev, GETSIZE(prev->size) + size + 3 * WORDSIZE + GETSIZE(next->size) + HEADSIZE, STATE_FREE);
    PUT(prev, k);
  }
}

void* kalloc(size_t size) {
  int k = rand() % BRANCH;
  spin_lock(&locks[k]);
  if (size == 0)
  {
    return NULL;
  }
  //fast path
  CHUNK *itr = head[k];
  //slow path 
  while (itr->next != head[k])
  {
    void * valid_start = alloc_split(itr->next, size);
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
  coalescing((size_t *)ptr, k);
  spin_unlock(&locks[k]);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
