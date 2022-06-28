#include <stdint.h>
typedef int spinlock_t;
void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);