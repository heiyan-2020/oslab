#ifdef LOCAL_MACHINE
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug()
#endif
// #define STACK_SIZE 65536
// #define POOL_SIZE 128
// #define ROUNDUP(a, sz)      ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
struct co* co_start(const char *name, void (*func)(void *), void *arg);
void co_yield();
void co_wait(struct co *co);
