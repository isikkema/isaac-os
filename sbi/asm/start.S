.section .text.init
.global _start
_start:
    csrr    a0, mhartid                 # Read mhartid

    la      sp, _stack_end

    li      t0, 4096                    # Set sp to stack_end + 4096 * hartid
    mul     t0, a0, t0
    sub     sp, sp, t0

    la      ra, park                    # Return to park() once we return form main

    la      t1, main                    # Put addr of main into
    csrw    mepc, t1                    # Return PC

    li      t1, (3 << 11) | (1 << 7)    # I had no clue this was allowed
    csrw    mstatus, t1

    li      t1, (1 << 11)               # Enable interrupts
    csrw    mie, t1

    la      t1, sbi_trap_vector
    csrw    mtvec, t1

    mret                                # "call" main

.global park
park:
    wfi         # Wait-For-Interrupt (Wii)
    j    park
