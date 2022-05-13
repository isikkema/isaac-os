#include <printf.h>
#include <os.h>

int main() {
    unsigned long i;
    for (i = 0; i < 10; i++) {
        printf("%d\n", i);

        sleep(10000000);
    }

    return 0;
}
