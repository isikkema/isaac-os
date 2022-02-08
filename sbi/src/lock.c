#include <lock.h>


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


int semaphore_trydown(Semaphore* semaphore) {
    int old;

    asm volatile("amoadd.w %0, %1, (%2)" : "=r"(old) : "r"(-1), "r"(&semaphore->value));

    if (old <= 0) {
        semaphore_up(semaphore);
    }

    return old > 0;
}

void semaphore_spindown(Semaphore* semaphore) {
    while (!semaphore_trydown(semaphore)) {
        // spin
    }
}

void semaphore_up(Semaphore* semaphore) {
    asm volatile("amoadd.w zero, %0, (%1)" :: "r"(1), "r"(&semaphore->value));
}


void barrier_spinwait(Barrier* barrier) {
    asm volatile("amoadd.w zero, %0, (%1)" :: "r"(-1), "r"(&barrier->value));

    while (barrier->value > 0) {
        // spin
    }
}
