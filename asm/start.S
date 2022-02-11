.section .text.init
.global _start
_start:
    la      sp, _stack_end
    la      gp, __global_pointer$

    la      ra, park                    # Return to park() once we return from main

    la      t1, main                    # Put addr of main into
    csrw    sepc, t1                    # Return PC

    li      t1, (1 << 13) | (1 << 8) | (1 << 5) 
    csrw    sstatus, t1

    li      t1, (1 << 9)                # Enable interrupts
    csrw    sip, t1

    la      t1, os_trap_vector          # Set stvec so trap_vector handles interrupts
    csrw    stvec, t1

    la      ra, park

    csrr    a0, sscratch

    sret                                # "call" main

.global park
park:
    wfi         # Wait-For-Interrupt (Wii)
    j    park
