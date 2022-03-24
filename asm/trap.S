.section .text
.altmacro

.macro savegp i
    sd  x\i, (8 * \i)(t6)
.endm

.macro loadgp i
    ld  x\i, (8 * \i)(t6)
.endm

.align 4
.global os_trap_vector
os_trap_vector:
    csrrw   t6, sscratch, t6

    # Save 30 gp regs frame
    .set i, 1
    .rept 30
        savegp %i
        .set i, i + 1
    .endr

    # Save last reg
    mv      t5, t6
    csrrw   t6, sscratch, t6
    sd      t6, 8*31(t5)

    # Go into C

    ### This fucks up the stack. Why did I do this?
    ### Call takes care of everything, and
    ### loadgp takes care of everything else.
    # la      sp, _stack_end
    # la      gp, __global_pointer$

    # Don't think we need multiple stacks
    # in any case, idk how without mhartid

    call    c_trap
    csrr    t6, sscratch

    # Load frame back
    .set i, 1
    .rept 31
        loadgp %i
        .set i, i + 1
    .endr

    sret
