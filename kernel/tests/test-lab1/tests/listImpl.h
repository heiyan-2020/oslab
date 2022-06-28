#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define HEAP_START 0x1000000
#define HEAP_END 0x8000000
//链表中的区间结构体定义
typedef struct interval{
    int l; //区间左端点
    int r; //区间右端点
    int idx; //在全局数组中的索引, 用于在O(1)释放/分配空间
    struct interval *next; //指向下一个节点
} Interval;

//最多维护多少个区间
#define INTERVALS_LIMIT (1 << 22)

// uintptr_t find_interval(size_t size);
void inter_init();
void *kalloc(size_t size);
void kfree(void *ptr);
