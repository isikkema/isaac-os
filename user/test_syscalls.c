#include <printf.h>


int main() {
    unsigned long i;
    volatile unsigned long j;
    for (i = 0; i < 1000000000; i++) {
        j = i / 7;

        if (i % 100000000 == 0) {
            printf("%d\n", i);
        }
    }

    return 0;
}
