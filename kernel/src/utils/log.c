#include <config.h>
#include <os.h>
int locked = 0;
void lock_() {
    int old = ienabled();
    iset(false);
    while (atomic_xchg(&locked, 1));
    if (old) iset(true);
}

void unlock_() {
    atomic_xchg(&locked, 0);
}
