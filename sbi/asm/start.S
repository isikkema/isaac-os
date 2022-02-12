.section .text.init
.global _start
_start:
    csrr    a0, mhartid                 # Read mhartid

    la      sp, _stack_end

    li      t0, 4096                    # Set sp to stack_end + 4096 * hartid
    mul     t0, a0, t0
    sub     sp, sp, t0

    tail    main

.global park
park:
    wfi         # Wait-For-Interrupt (Wii)
    j    park
