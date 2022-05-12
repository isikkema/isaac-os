int main() {
    unsigned long i;
    volatile unsigned long j;
    for (i = 0; i < 100000000; i++) {
        j = i / 7;
    }

    return 0;
}
