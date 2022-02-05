.section .text
.altmacro

.macro savegp i
    sd  x\i, (8 * \i)(t6)
.endm

.macro loadgp i
    ld  x\i, (8 * \i)(t6)
.endm

.align 4
.global sbi_trap_vector
sbi_trap_vector:
    csrrw   t6, mscratch, t6

    # Save 30 gp regs frame
    .set i, 1
    .rept 30
        savegp %i
        .set i, i + 1
    .endr

    # Save last reg
    mv      t5, t6
    csrrw   t6, mscratch, t6
    sd      t6, 8*31(t5)

    # Go into C
    la      sp, _stack_end
    call    c_trap
    csrr    t6, mscratch

    # Load frame back
    .set i, 1
    .rept 31
        loadgp %i
        .set i, i + 1
    .endr

    mret
