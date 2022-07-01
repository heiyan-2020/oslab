//一些宏定义
#ifndef PARAM_H
#define PARAM_H
#define NCPU 8
#define NTASK 64
#define PID_BOUND 32767
#define NPAGES 64


#define KiB 1 << 10
#define MiB 1 << 20

#define STACK_SIZE (2 * KiB)
#define MAGIC_NUM -2848
#define MAP_SHARED 0x1
#define MAP_PRIVATE 0x2
#define MAP_UNMAP 0x3
#endif