// CSR_READ(variable, "register"). Must use quotes for the register name.
#define CSR_READ(var, csr)    asm volatile("csrr %0, " csr : "=r"(var))

// CSR_WRITE("register", variable). Must use quotes for the register name.
#define CSR_WRITE(csr, var)   asm volatile("csrw " csr ", %0" :: "r"(var))
