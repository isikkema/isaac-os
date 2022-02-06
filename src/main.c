#include <printf.h>


int main(int hart) {
    printf("printing in OS main on hart %d\n", hart);
    while (1) {};
    return 0;
}
