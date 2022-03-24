#include <csr.h>
#include <printf.h>


void c_trap(void) {
    printf("HIT OS C_TRAP!\n");
    
    while (1) WFI();
}
