typedef struct Mutex {
    int state;
} Mutex;


int mutex_trylock(Mutex* mutex);
void mutex_spinlock(Mutex* mutex);
void mutex_unlock(Mutex* mutex);
