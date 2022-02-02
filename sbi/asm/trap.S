.section .text

.align 4
.global sbi_trap_vector
sbi_trap_vector:
    call c_trap
