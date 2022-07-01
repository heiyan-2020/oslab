#include <pmm.h>
#ifdef TEST
#include <assert.h>
#include <string.h>
#endif
/*============================================
                  lock
=============================================*/
void lock(lock_t *lk) {
  while (atomic_xchg(lk, 1));
}

void unlock(lock_t *lk) {
  atomic_xchg(lk, 0);
}

/*============================================
          increment global memory space
=============================================*/
static void *heap_ptr = NULL;
static void *upper_limit = NULL;
lock_t heap_lk;
static void *pmm_brk(size_t size) {
  lock(&heap_lk);
  size_t pow2 = round_power2(size);
  void *old = (void *)ROUNDUP(heap_ptr, pow2);
  heap_ptr = old + size;
  if (heap_ptr > upper_limit) {
    unlock(&heap_lk);
    return NULL;
  }
  unlock(&heap_lk);
  return old;
}
/*============================================
                initialize
=============================================*/
heap_t tlhs[MAX_THREADS];
/*initialize a new page*/
void init_page(page_t *page, void* field, int page_blocksize, size_t size) {
  page->page_blocksize = page_blocksize;
  size_t cnt = 0;
  // page->page_lock = 0;
  page->local_free = NULL;
  void *start = (void *)ROUNDUP(field, page_blocksize);
  cnt += start - field + page_blocksize;
  page->free = (block_t*)start;
  start += page_blocksize;
  while (cnt < size)
  {
    block_t *tmp = (block_t*)start;
    tmp->next = page->free;
    page->free = tmp;
    cnt += page_blocksize;
    start += page_blocksize;
  }
}

/*initialize a new slab*/
slab_t* init_slab(int page_kind, int page_blocksize) {
  slab_t *slab = (slab_t*) pmm_brk(SLAB_SIZE);
  if (slab == NULL) return NULL;
  slab->page_kind = page_kind;
  slab->magic_number = MAGIC;
  int pages_count = 0;
  switch (page_kind)
  {
  case SMALL:
  case MID: {
    slab->page_shift = MID_PAGE_WIDTH;
    pages_count = SMALL_PAGES_PER_SLAB;
    break;
  }
  case LARGE: {
    slab->page_shift = LARGE_PAGE_WIDTH;
    pages_count = LARGE_PAGES_PER_SLAB;
  }
  default:
    break;
  }
  size_t page_size = 1 << (slab->page_shift);
  void *first_page = (void*)slab + sizeof(slab_t) + sizeof(page_t) * (pages_count - 1); //因为slab中有一个占位的，所以减一
  void *free_field = (void*)slab + (1 << (slab->page_shift));
  init_page(&slab->pages[0], first_page, page_blocksize, (size_t)(free_field - first_page));
  for (int i = 1; i < pages_count; i++) {
    init_page(&slab->pages[i], free_field, page_blocksize, page_size);
    slab->pages[i - 1].next = &slab->pages[i];
    free_field += page_size;
  }
  return slab;
}
/*create a new thread-local heap*/

void init_heap(int thread_id) {
  heap_t *iheap = &tlhs[thread_id];
  // printf("iheap=%ld\n", iheap);
  iheap->thread_id = thread_id;
  // assert(iheap == NULL);
  int page_cnt = 0;
  /*add small pages*/
  for (int i = SMALL_BOUND; i <= SMALL_SIZE; i *= 2) {
    slab_t* islab = init_slab(SMALL, i);
    if (islab == NULL) return;
    iheap->pages[page_cnt] = &islab->pages[0];
    iheap->pages_dir[page_cnt++] = &islab->pages[0];
  }
  /*add mid pages*/
  for (int i = (SMALL_SIZE << 1); i <= MID_SIZE; i *= 2) {
    slab_t* islab = init_slab(MID, i);
    if (islab == NULL) return;
    iheap->pages[page_cnt] = &islab->pages[0];
    iheap->pages_dir[page_cnt++] = &islab->pages[0];
  }
  /*add large pages*/
  for (int i = (MID_SIZE << 1); i <= LARGE_SIZE; i *= 2) {
    slab_t* islab = init_slab(LARGE, i);
    if (islab == NULL) return;
    iheap->pages[page_cnt] = &islab->pages[0];
    iheap->pages_dir[page_cnt++] = &islab->pages[0];
  }
  // snapShot(iheap);
}
#ifndef TEST
static void pmm_init() {
  heap_ptr = (void*)heap.start;
  upper_limit = (void*)heap.end;
  memset(heap_ptr, 0, (size_t)(upper_limit - heap_ptr));
  // printf("heap size=%ldMiB\n", (uintptr_t)(upper_limit - heap_ptr) >> 20);
  for (int i = 0; i < cpu_count(); i++) {
    init_heap(i);
  }
}
#else
static void pmm_init() {
  heap_ptr = malloc(1 << 27);
  memset(heap_ptr, 0, 1 << 27);
  upper_limit = heap_ptr + (1 << 27);
  for (int i = 0; i < cpu_count(); i++) {
    init_heap(i);
  }
}
#endif

/*============================================
                allocate
=============================================*/

void *malloc_generic(size_t size, heap_t* hp, int idx) {
    // snapShot(hp);
  page_t *itr = hp->pages[idx];
  //mid path. pages中还有未满的page
  while (itr != NULL)
  {
    if (itr->free == NULL) {
      lock(&itr->page_lock);
      itr->free = itr->local_free;
      // printf("itr->free=%ld\n", itr->free);
      itr->local_free = NULL;
      unlock(&itr->page_lock);
    }

    if (itr->free != NULL) {
      void *block = (void*)itr->free;
      itr->free = itr->free->next;
      hp->pages_dir[idx] = itr;
      // printf("block=%ld\n", block);
      return block;
    }
    itr = itr->next;
  }
  //slow path. hp现有的pages全满(i.e. slab全满), 需要分配新的slab
  printf("slow path\n");
  int kind, pages_count;
  if (size <= SMALL_SIZE) {
    kind = SMALL;
    pages_count = SMALL_PAGES_PER_SLAB;
  } else if (size <= MID_SIZE) {
    kind = MID;
    pages_count = MID_PAGES_PER_SLAB;
  } else {
    kind = LARGE;
    pages_count = LARGE_PAGES_PER_SLAB;
  }
  slab_t *another_slab = init_slab(kind, size);
  if (another_slab == NULL) return NULL;
  another_slab->pages[pages_count - 1].next = hp->pages[idx];
  hp->pages[idx] = &another_slab->pages[0];
  hp->pages_dir[idx] = hp->pages[idx];
  void *ptr = hp->pages[idx]->free;
  hp->pages[idx]->free = hp->pages[idx]->free->next;
    // snapShot(hp);
  return ptr;
}

// void *small_alloc(size_t size, heap_t *tlh) {
//   assert(size <= SMALL_SIZE);
//   size = ROUNDUP(size, SMALL_BOUND);
//   page_t *page = tlh->pages_dir[(size + 7) >> 3];
//   block_t *block = page->free;
//   if (block == NULL) return malloc_generic(size, tlh, (size + 7) >> 3); //mid path.
//   page->free = block->next;
//   assert(page->free != NULL);
//   return (void*)block;
// }
// void snapShot(heap_t *hp) {
//   for (int i = 0; i < LARGE_WIDTH - 2; i++) {
//     page_t *itr = hp->pages[i];
//     while (itr != NULL)
//     {
//       printf("idx=%d\n", i);
//       assert(itr != 0x10);
//       block_t *block = itr->free;
//       while (block != NULL)
//       {
//         assert(block != 0x10);
//         block = block->next;
//       }
      
//       itr = itr->next;
//     }
//   }
// }

void *mid_alloc(size_t size, heap_t *tlh) {
  if (size == 0 || size >= (1 << 24)) {
    return NULL;
  }
  size = size >= 8 ? size : 8;
  size = round_power2(size);
  size_t tmp = size;
  int idx = 0;
  while (tmp > 0)
  {
    tmp >>= 1;
    idx++;
  }
  idx -= 1;
  page_t *page = tlh->pages_dir[idx - 3];
  block_t *block = page->free;
  if (block == NULL) return malloc_generic(size, tlh, idx - 3); //mid path.
  page->free = block->next; 
  // snapShot(tlh);
  return (void*)block;
}

void *huge_alloc(size_t size) {
  return pmm_brk(size);
}

void* kalloc(size_t size) {
  heap_t *tlh = &tlhs[cpu_current()];
  if (size <= LARGE_SIZE) {
    return mid_alloc(size, tlh);
  }
  return huge_alloc(size);
}

/*============================================
                    free
=============================================*/
void kfree(void *ptr) {
  slab_t *slab_ = (slab_t *)((uintptr_t)ptr & ~(SLAB_SIZE - 1));
  if (slab_ == NULL || slab_->magic_number != MAGIC) {
    return;
  }
  page_t *page = &slab_->pages[(uintptr_t)(ptr - (void*)slab_) >> slab_->page_shift];
  block_t *block = (block_t*)ptr;
  if (cpu_current() == slab_->thread_id) {
    block->next = page->local_free;
    page->local_free = block;
  } else {
    lock(&page->page_lock);    //其它线程，需要上锁
    block->next = page->local_free;
    page->local_free = block;
    unlock(&page->page_lock);
  }
}

static void *kalloc_safe(size_t size) {
  int i = ienabled();
  iset(false);
  void *ret = kalloc(size);
  panic_on(ret == NULL, "return NUll pointer\n");
  printf("alloc %d bytes on %p\n", size, ret);
  if (i) iset(true);
  return ret;
}

static void kfree_safe(void *ptr) {
  int i = ienabled();
  iset(false);
  kfree(ptr);
  printf("free %p\n", ptr);
  if (i) iset(true);
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc_safe,
  .free  = kfree_safe,
};