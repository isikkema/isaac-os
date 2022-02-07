#include <mutex.h>


int mutex_trylock(Mutex* mutex) {
    int old;

    asm volatile("amoswap.w.aq %0, %1, (%2)" : "=r"(old) : "r"(1), "r"(&mutex->state));

    return old-1;
}

void mutex_spinlock(Mutex* mutex) {
    while (!mutex_trylock(mutex)) {
        // spin
    }
}

void mutex_unlock(Mutex* mutex) {
    asm volatile("amoswap.w.rl zero, zero, (%0)" :: "r"(&mutex->state));
}
