#include <utils.h>
#include <printf.h>


int atoi(char* str) {
    int num;
    int tens;
    int digit;
    int i;

    printf("{%s}\n", str);

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
