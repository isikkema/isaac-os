# .section .text.init
# .global _start
# _start:
#     # csrr    a0, mhartid                 # Read mhartid

#     la      sp, _stack_end
#     la      gp, __global_pointer$

#     li      t0, 4096                    # Set sp to stack_end + 4096 * hartid
#     mul     t0, a0, t0
#     sub     sp, sp, t0

#     la      ra, park                    # Return to park() once we return form main

#     la      t1, main                    # Put addr of main into
#     csrw    sepc, t1                    # Return PC

#     li      t1, (1 << 13) | (1 << 8) | (1 << 5)    # I had no clue this was allowed
#     csrw    sstatus, t1

#     li      t1, (1 << 9)               # Enable interrupts
#     csrw    sip, t1

#     la      t1, sbi_trap_vector         # Set mtvec so sbi_trap_vector handles interrupts
#     csrw    stvec, t1

#     sret                                # "call" main

# .global park
# park:
#     wfi         # Wait-For-Interrupt (Wii)
#     j    park
