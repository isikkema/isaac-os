#include <string.h>


int strlen(char s[]) {
    int i;

    for (i = 0; s[i] != '\0'; i++) {};

    return i;
}

int strcmp(char a[], char b[]) {
    int a_len, b_len;
    int i;

    a_len = strlen(a);
    b_len = strlen(b);
    
    if (a_len < b_len) {
        return -1;
    } else if (a_len > b_len) {
        return 1;
    }

    for (i = 0; i < a_len; i++) {
        if (a[i] < b[i]) {
            return -1;
        } else if (a[i] > b[i]) {
            return 1;
        }
    }

    return 0;
}
