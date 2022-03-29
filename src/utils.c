#include <utils.h>
#include <string.h>


int atoi(char* str) {
    int num;
    int tens;
    int digit;
    int i;

    tens = 1;
    num = 0;
    for (i = strlen(str)-1; i >= 0; i--) {
        if (str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
            num += digit * tens;

            tens *= 10;
        } else {
            return 0;
        }
    }

    return num;
}

long atol(char* str) {
    long num;
    long tens;
    long digit;
    int i;

    tens = 1;
    num = 0;
    for (i = strlen(str)-1; i >= 0; i--) {
        if (str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
            num += digit * tens;

            tens *= 10;
        } else {
            return 0;
        }
    }

    return num;
}
