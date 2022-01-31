.section .text.init
.global _start
_start:
    csrr  a0, mhartid
# .option push
# .option norelax
    la    sp, _stack_end
    # la    gp, __global_pointer$
# .option pop
    li    t0, 4096
    mul   t0, a0, t0
    sub   sp, sp, t0
    call  main

.global park
park:
    wfi  // Wait-For-Interrupt (Wii)
    j    park
