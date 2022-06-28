#include <common.h>
#include <rand.h>
#include <lock.h>
#include <stdint.h>
#include <stddef.h>
size_t HEAPSIZE;
#define HEADSIZE (sizeof(CHUNK))
#define WORDSIZE (sizeof(size_t))
#define LOWER_BOUND (HEADSIZE + WORDSIZE)
#define BRANCH (32)
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
#define BOUND_ALLOC(p) (void*)((void*)p + (GETSIZE(*((size_t*)p - 1)))) //p is size_t* or void*

spinlock_t locks[BRANCH];
spinlock_t global_lk;

typedef struct ChunkHead {
    size_t size;// 高位为size, 最低1位为标志位 1代表free 0代表allocated
    struct ChunkHead *prev;
    struct ChunkHead *next;
} CHUNK;
CHUNK *head[BRANCH];
CHUNK *next_fit[BRANCH];
int seg[BRANCH];

#define CPU_NUM 2
int freelist[CPU_NUM]; //cpu_no -> branch_no

typedef struct page
{
  int cpu_no;
  size_t busyByes;
} PAGE;

PAGE pages[BRANCH];

static inline void PUT(CHUNK* chunk, int k) {
  chunk->next = head[k]->next;
  head[k]->next->prev = chunk;
  head[k]->next = chunk;
  chunk->prev = head[k];
}

static inline void update_size(CHUNK* chunk, size_t size, int state) {
  // //assert(chunk >= head[0]);
  SETSIZE(chunk->size, size);
  SET_STATE(chunk->size, state);
  size_t *boundary_sizePtr = (size_t*)BOUNDARY(chunk);
  *boundary_sizePtr = size << 2;
  // SETSIZE(*boundary_sizePtr, size);
  SET_STATE(*boundary_sizePtr, state);
}

static inline CHUNK* getFromBound(size_t* bound) {
  ////assert(GET_STATE(*bound) == STATE_FREE);
  void *start = (void *)bound - GETSIZE(*bound) - HEADSIZE;
  return (CHUNK *)start;
}

static inline size_t* getFromAllocBound(size_t* bound) {
  ////assert(GET_STATE(*bound) == STATE_ALLOC);
  void *start = (void *)bound - GETSIZE(*bound) - WORDSIZE;
  return (size_t *)start;
}

void snapShot() {
  CHUNK *itr = head[0];
  printf("=================================\n");
  printf("head[0]->prev->size=0x%lx\n", GETSIZE(head[0]->prev->size));
  printf("next_fit[0]=%p\n", next_fit[0]);
  while (itr->next != head[0])
  {
    printf("itr=%p, itr->prev=%p, itr->next=%p\n", itr, itr->prev, itr->next);
    itr = itr->next;
  }
}
#ifndef TEST
// 框架代码中的 pmm_init (在 AbstractMachine 中运行)
// #define //assert(s) //##s
static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  //init
  void* base = heap.start;
  HEAPSIZE = (heap.end - heap.start) / BRANCH;
  for (int i = 0; i < BRANCH; i++)
  {
    head[i] = base + HEAPSIZE * i;
    CHUNK *first = (void*)head[i] + (HEADSIZE << 1);
    head[i]->next = first;
    head[i]->prev = first;
    first->prev = head[i];
    first->next = head[i];
    seg[i] = 1 << 12;
    pages[i].cpu_no = -1;
    SETSIZE(head[i]->size, ((HEADSIZE - WORDSIZE) << 1));
    SET_STATE(head[i]->size, STATE_ALLOC);
    size_t *head_bound = (size_t *)first - 1;
    SETSIZE(*head_bound, ((HEADSIZE - WORDSIZE) << 1));
    SET_STATE(*head_bound, STATE_ALLOC);
    update_size(first, HEAPSIZE - (HEADSIZE * 3) - WORDSIZE, STATE_FREE);
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
    next_fit[i] = head[i];
    seg[i] = 1 << 12;
    SETSIZE(head[i]->size, ((HEADSIZE - WORDSIZE) << 1));
    SET_STATE(head[i]->size, STATE_ALLOC);
    size_t *head_bound = (size_t *)first - 1;
    SETSIZE(*head_bound, ((HEADSIZE - WORDSIZE) << 1));
    SET_STATE(*head_bound, STATE_ALLOC);
    update_size(first, HEAPSIZE - (HEADSIZE * 3) - WORDSIZE, STATE_FREE);
  }

  for (int i = 0; i < CPU_NUM; i++) {
    freelist[i] = i;
    pages[i].cpu_no = i;
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

void* alloc_split(CHUNK* chunk, size_t size, size_t size_bound) {
  // //assert(GET_STATE(chunk->size) == STATE_FREE);
  // printf("chunk=%p, prev.size=0x%lx, prevBound=%p\n", chunk, GETSIZE(*((size_t *)chunk - 1)), (size_t *)chunk - 1);
  // //assert(GET_STATE(*((size_t *)chunk - 1)) == STATE_ALLOC);
  size_t alignment = round_power2(size);
  size = size < (HEADSIZE - WORDSIZE) ? HEADSIZE - WORDSIZE : size;
  size = (size + WORDSIZE - 1) & ~(WORDSIZE - 1); //与字长对齐
  // //assert(size % WORDSIZE == 0);
  // void *start = (void *)chunk + WORDSIZE;
  void *start = (void*)ROUNDUP((uintptr_t *)chunk + WORDSIZE, alignment);
  size_t front_gap = start - (void *)chunk - WORDSIZE;
  if ((void*)BOUNDARY(chunk) <= start) {
    return NULL;
  }
  size_t back_gap = BOUNDARY(chunk) - start;
  // //assert(front_gap % WORDSIZE == 0);
  if (back_gap >= size) {
    if (front_gap < LOWER_BOUND + size_bound && back_gap < LOWER_BOUND + size + size_bound) 
    {
      // printf("alloc case 1\n");
      chunk->prev->next = chunk->next;
      chunk->next->prev = chunk->prev;
      size_t *prevBound = (size_t *)chunk - 1;
      size_t new_size = GETSIZE(*prevBound) + front_gap;
      size_t *prevHead = getFromAllocBound(prevBound);
      SETSIZE(*prevHead, new_size);
      size_t *new_bound = (size_t *)start - 2;
      SETSIZE(*new_bound, new_size);
      SET_STATE(*new_bound, STATE_ALLOC);

      size_t *new_allocHead = (size_t *)start - 1;
      SETSIZE(*new_allocHead, back_gap);
      SET_STATE(*new_allocHead, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, back_gap);
      SET_STATE(*tt, STATE_ALLOC);
    } 
    else if (front_gap >= LOWER_BOUND + size_bound && back_gap < LOWER_BOUND + size + size_bound) 
    {
            // printf("alloc case 2\n");

      SETSIZE(chunk->size, (front_gap - LOWER_BOUND));
      SET_STATE(chunk->size, STATE_FREE);
      size_t *new_bound = (size_t *)BOUNDARY(chunk);
      // //assert(new_bound == (size_t *)start - 2);
      SETSIZE(*new_bound, (front_gap - LOWER_BOUND));
      SET_STATE(*new_bound, STATE_FREE);

      size_t *new_allocHead = (size_t *)start - 1;
      SETSIZE(*new_allocHead, back_gap);
      SET_STATE(*new_allocHead, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, back_gap);
      SET_STATE(*tt, STATE_ALLOC);
    }
    else if (front_gap < LOWER_BOUND + size_bound && back_gap >= LOWER_BOUND + size + size_bound)
    {
      // printf("alloc case 3\n");

      CHUNK *new_chunk = (CHUNK *)(start + size + WORDSIZE);
      new_chunk->prev = chunk->prev;
      new_chunk->next = chunk->next;
      chunk->prev->next = new_chunk;
      chunk->next->prev = new_chunk;
      // //assert(back_gap >= size + LOWER_BOUND);
      update_size(new_chunk, back_gap - size - LOWER_BOUND, STATE_FREE);

      size_t *prevBound = (size_t *)chunk - 1;
      size_t new_size = GETSIZE(*prevBound) + front_gap;
      size_t *prevHead = getFromAllocBound(prevBound);
      SETSIZE(*prevHead, new_size);
      size_t *new_bound = (size_t *)start - 2;
      SETSIZE(*new_bound, new_size);
      SET_STATE(*new_bound, STATE_ALLOC);

      size_t *new_head = (size_t *)start - 1;
      SETSIZE(*new_head, size);
      SET_STATE(*new_head, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, size);
      SET_STATE(*tt, STATE_ALLOC);
    }
    else
    {
            // printf("alloc case 4\n");

      //assert(front_gap >= LOWER_BOUND + size_bound && back_gap >= LOWER_BOUND + size + size_bound);
      SETSIZE(chunk->size, (front_gap - LOWER_BOUND));
      SET_STATE(chunk->size, STATE_FREE);
      size_t *new_bound = (size_t *)BOUNDARY(chunk);
      // printf("cunnk=%p, size=0x%lx\n",chunk ,front_gap - LOWER_BOUND);
      //assert(new_bound == (size_t *)start - 2);
      SETSIZE(*new_bound, (front_gap - LOWER_BOUND));
      SET_STATE(*new_bound, STATE_FREE);

      size_t *new_allocHead = (size_t *)start - 1;
      SETSIZE(*new_allocHead, size);
      SET_STATE(*new_allocHead, STATE_ALLOC);
      size_t *tt = (size_t*)BOUND_ALLOC(start);
      SETSIZE(*tt, size);
      SET_STATE(*tt, STATE_ALLOC);

      CHUNK *new_chunk = (CHUNK *)(start + size + WORDSIZE);
      new_chunk->next = chunk->next;
      chunk->next->prev = new_chunk;
      chunk->next = new_chunk;
      new_chunk->prev = chunk;
      // //assert(back_gap >= size + LOWER_BOUND);
      update_size(new_chunk, back_gap - size - LOWER_BOUND, STATE_FREE);
      // printf("new_chunk=%p, size=%lx\n", new_chunk, back_gap - size - LOWER_BOUND);
    }
    // printf("allocHead=%p\n", start - WORDSIZE);
    return start;
  }
  return NULL;
}


void coalescing(size_t *freeptr, int k) {
  // printf("freeptr=%p, boundary=%p\n", freeptr, BOUND_ALLOC(freeptr));
  int prev_state = GET_STATE(*(freeptr - 2));
  // printf("freeptr=%p, *=0x%lx\n", freeptr - 1, *(freeptr - 1));
  int next_state = GET_STATE(*((size_t *)(BOUND_ALLOC(freeptr)) + 1));
  size_t size = GETSIZE(*(freeptr - 1));
  CHUNK *chunk = (CHUNK *)(freeptr - 1);
  if (prev_state == STATE_ALLOC && next_state == STATE_ALLOC)
  {
    // printf("coalescing case 1\n");
    update_size(chunk, size - (HEADSIZE - WORDSIZE), STATE_FREE);
    PUT(chunk, k);
  } else if (prev_state == STATE_FREE && next_state == STATE_ALLOC) {
    // printf("coalescing case 2\n");

    CHUNK *prev = getFromBound(freeptr - 2);
    prev->prev->next = prev->next;
    prev->next->prev = prev->prev;
    update_size(prev, GETSIZE(prev->size) + size + 2 * WORDSIZE, STATE_FREE);
    PUT(prev, k);
  } else if (prev_state == STATE_ALLOC && next_state == STATE_FREE) {
        // printf("coalescing case 3\n");

    CHUNK *next = (CHUNK *)((size_t *)BOUND_ALLOC(freeptr) + 1);
    next->prev->next = next->next;
    next->next->prev = next->prev;
    CHUNK *new_head = (CHUNK *)(freeptr - 1);
    update_size(new_head, GETSIZE(next->size) + size + 2 * WORDSIZE, STATE_FREE);
    PUT(new_head, k);
    if (next_fit[k] == next) {
      next_fit[k] = new_head;
    }
  } else if (prev_state == STATE_FREE && next_state == STATE_FREE) {
        // printf("coalescing case 4\n");
    CHUNK *prev = getFromBound(freeptr - 2);
    CHUNK *next = (CHUNK*)((size_t *)BOUND_ALLOC(freeptr) + 1);
    // printf("this=%p, prev=%p,prev->size=0x%lx ,next=%p\n", freeptr - 1, prev,GETSIZE(prev->size ) ,next);

    prev->prev->next = prev->next;
    prev->next->prev = prev->prev;
    next->prev->next = next->next;
    next->next->prev = next->prev;
    update_size(prev, GETSIZE(prev->size) + size + 3 * WORDSIZE + GETSIZE(next->size) + HEADSIZE, STATE_FREE);
    PUT(prev, k);
    if (next_fit[k] == next) {
      next_fit[k] = prev;
    }
  }
}

int getFreeList(int cpu_no) {
  size_t min = HEAPSIZE / BRANCH;
  int idx = 0;
  spin_lock(&global_lk);
  for (int i = 0; i < BRANCH; i++)
  {
    if (pages[i].cpu_no == -1 && pages[i].busyByes < min) {
      min = pages[i].busyByes;
      idx = i;
    }
  }
  pages[freelist[cpu_no]].cpu_no = -1;
  freelist[cpu_no] = idx;
  pages[idx].cpu_no = cpu_no;
  spin_unlock(&global_lk);
  return idx;
}

void* kalloc(size_t size, int cpu_no) {
  if (size == 0 || size > (1 << 24))
  {
    return NULL;
  }
  int k = freelist[cpu_no];
  spin_lock(&locks[k]);
  CHUNK* itr = head[k];
  while (itr->next != head[k])
  {
    void *valid_start = alloc_split(itr->next, size, seg[k] - (HEADSIZE - WORDSIZE));
    if (valid_start != NULL)
    {
      pages[k].busyByes += size + (HEADSIZE - WORDSIZE);
      spin_unlock(&locks[k]);
      return valid_start;
    }
    itr = itr->next;
  }
  spin_unlock(&locks[k]);
  k = getFreeList(cpu_no);
  spin_lock(&locks[k]);
  itr = head[k];
  while (itr->next != head[k])
  {
    void *valid_start = alloc_split(itr->next, size, seg[k] - (HEADSIZE - WORDSIZE));
    if (valid_start != NULL)
    {
      pages[k].busyByes += size + (HEADSIZE - WORDSIZE);
      spin_unlock(&locks[k]);
      return valid_start;
    }
    itr = itr->next;
  }
  spin_unlock(&locks[k]);
  return NULL;
}

void kfree(void* ptr) {
  if (ptr == NULL) {
    return;
  }
  int k = (ptr - (void *)head[0]) / HEAPSIZE;
  spin_lock(&locks[k]);
  size_t sz = GETSIZE(*((size_t *)ptr - 1));
  coalescing((size_t *)ptr, k);
  pages[k].busyByes = pages[k].busyByes >= sz ? pages[k].busyByes - sz : 0;
  spin_unlock(&locks[k]);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
