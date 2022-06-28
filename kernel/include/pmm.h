#include <stddef.h>
#include <stdint.h>
#include <os.h>
/*==================================================
                    configuration
===================================================*/
/*define the slab*/
#define SLAB_WIDTH 20
#define SLAB_SIZE (1 << SLAB_WIDTH)
#define SMALL_PAGES_PER_SLAB (SLAB_SIZE / SMALL_PAGE_SIZE)
#define MID_PAGES_PER_SLAB (SLAB_SIZE / MID_PAGE_SIZE)
#define LARGE_PAGES_PER_SLAB (SLAB_SIZE / LARGE_PAGE_SIZE)
#define MAGIC 0x12345678


/*define the page*/
enum page_kind {
    SMALL = 0, MID, LARGE, HUGE
};
#define SMALL_BOUND (8) //the smallest block size
#define SMALL_WIDTH 8
#define MID_WIDTH 13
#define LARGE_WIDTH 19
#define HUGE_WIDTH 24
#define SMALL_SIZE (1 << SMALL_WIDTH)
#define MID_SIZE (1 << MID_WIDTH)
#define LARGE_SIZE (1 << LARGE_WIDTH)
#define HUGE_SIZE (1 << HUGE_WIDTH)
#define MID_PAGE_WIDTH 16
#define LARGE_PAGE_WIDTH SLAB_WIDTH
#define SMALL_PAGE_SIZE (1 << MID_PAGE_WIDTH)
#define MID_PAGE_SIZE (1 << MID_PAGE_WIDTH)
#define LARGE_PAGE_SIZE SLAB_SIZE


/*define the multi-thread*/
#define MAX_THREADS 8

/*==================================================
                type & datastructure
===================================================*/
typedef int lock_t;
typedef struct block {
    struct block *next;
} block_t;

typedef struct page {
    int page_blocksize;
    block_t *free;
    block_t *local_free;
    struct page *next;
    lock_t page_lock;
} page_t;

typedef struct slab {
    int page_shift;
    int page_kind;
    int magic_number;
    int thread_id;
    page_t pages[1]; //由于slab位于一块连续内存的首端, 因此这里只是占位， 数组具体有多少项运行时确定
} slab_t;

typedef struct heap {
    page_t *pages_dir[LARGE_WIDTH - 2];
    page_t *pages[LARGE_WIDTH - 2];
    int thread_id;
} heap_t;

/*==================================================
                macro & function
===================================================*/
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1)) //将a对齐到最近的sz. sz必须是2的幂

static inline size_t round_power2(size_t x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}