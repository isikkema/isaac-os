typedef struct Mutex {
    int state;
} Mutex;

typedef struct Semaphore {
    int value;
} Semaphore;

typedef struct Barrier {
    int value;
} Barrier;


int mutex_trylock(Mutex* mutex);
void mutex_sbi_lock(Mutex* mutex);
void mutex_unlock(Mutex* mutex);

int semaphore_trydown(Semaphore* semaphore);
void semaphore_sbi_down(Semaphore* semaphore);
void semaphore_up(Semaphore* semaphore);

void barrier_sbi_wait(Barrier* barrier);
void barrier_release(Barrier* barrier);
