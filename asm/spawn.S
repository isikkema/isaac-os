.altmacro

.macro savegp i
    sd  x\i, (8 * \i)(t6)
.endm

.macro loadgp i
    ld  x\i, (8 * \i)(t6)
.endm

.macro savefp i
    fsd  f\i, (8 * \i + 256)(t6)
.endm

.macro loadfp i
    fld  f\i, (8 * \i + 256)(t6)
.endm

.section .text
process_spawn:
    wfi

    csrrw   t6, sscratch, t6

    # # Save 30 gp regs into frame
    # .set i, 1
    # .rept 30
    #     savegp %i
    #     .set i, i + 1
    # .endr

    # # Save all fp regs into frame
    # .set i, 1
    # .rept 31
    #     savefp %i
    #     .set i, i + 1
    # .endr

    # # Save last gp reg into frame
    # mv      t5, t6
    # csrrw   t6, sscratch, t6
    # sd      t6, 8*31(t5)

    # gpregs      0
    # fpregs      256
    # sepc        512
    # sstatus     520
    # sie         528
    # satp        536
    # sscratch    544
    # stvec       552
    # trap_satp   560
    # trap_stack  568

    ld      t0, 512(t6)
    csrw    sepc, t0

    ld      t0, 520(t6)
    csrw    sstatus, t0

    ld      t0, 528(t6)
    csrw    sie, t0

    # We store the paddr of the frame in frame->sscratch
    # We need to load that, swap to kernel pt,
    # and then use the new sscratch to do future frame stuff
    ld      t1, 536(t6)

    ld      t0, 552(t6)
    csrw    stvec, t0

    ld      t0, 544(t6)
    csrw    sscratch, t0

    csrw    satp, t1

    sret

.align 4
process_spawn_end:

.section .rodata
.global process_spawn_addr
.global process_spawn_size
process_spawn_addr: .dword process_spawn
process_spawn_size: .dword process_spawn_end - process_spawn
