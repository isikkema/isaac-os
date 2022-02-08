typedef struct Mutex {
    int state;
} Mutex;

typedef struct Semaphore {
    int value;
} Semaphore;


int mutex_trylock(Mutex* mutex);
void mutex_spinlock(Mutex* mutex);
void mutex_unlock(Mutex* mutex);

int semaphore_trydown(Semaphore* semaphore);
void semaphore_spindown(Semaphore* semaphore);
void semaphore_up(Semaphore* semaphore);
