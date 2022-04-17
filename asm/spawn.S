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
    # We're dealing with kernel mmu here
    
    csrrw   t6, sscratch, t6

    # t6: kernel frame

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

    # We store the vaddr of the frame in frame->sscratch
    # We need to load that, swap to process pt,
    # and then use the new sscratch to do future frame stuff

    # process satp
    ld      t1, 536(t6)

    ld      t0, 552(t6)
    csrw    stvec, t0

    # process frame
    ld      t0, 544(t6)
    csrw    sscratch, t0

    # kernel frame
    sd      t6, 544(t6)

    # Now deal with process mmu
    csrw    satp, t1

    # process frame
    mv      t6, t0

    # Load frame
    .set i, 1
    .rept 31
        loadfp %i
        .set i, i + 1
    .endr

    .set i, 1
    .rept 31
        loadgp %i
        .set i, i + 1
    .endr

    sret

.align 4
process_spawn_end:


.align 4
process_trap_vector:
    # Dealing with process mmu
    csrrw   t6, sscratch, t6

    # Save 30 gp regs into frame
    .set i, 1
    .rept 30
        savegp %i
        .set i, i + 1
    .endr

    # Save all fp regs into frame
    .set i, 1
    .rept 31
        savefp %i
        .set i, i + 1
    .endr

    # Save last gp reg into frame
    mv      t5, t6
    csrrw   t6, sscratch, t6
    sd      t6, 8*31(t5)

    # trap satp
    ld      t0, 560(t5)

    # sscratch (kernel frame addr)
    ld      t6, 544(t5)

    # sp
    ld      sp, 568(t5)

    # process frame addr
    sd      t5, 544(t5)

    # Now dealing with kernel mmu
    csrw    satp, t0
    
    csrw    sscratch, t6

    # Go into C
    call c_trap

    csrr    t5, sscratch
    
    # process satp
    ld      t0, 536(t5)
    
    # process frame addr
    ld      t6, 544(t5)

    # kernel frame addr
    sd      t5, 544(t5)

    # Now dealing with process mmu
    csrw    satp, t0
    
    csrw    sscratch, t6

    # Load frame back
    .set i, 1
    .rept 31
        loadfp %i
        .set i, i + 1
    .endr

    .set i, 1
    .rept 31
        loadgp %i
        .set i, i + 1
    .endr

    sret

.align 4
process_trap_vector_end:


.section .rodata
.global process_spawn_addr
.global process_spawn_size
.global process_trap_vector_addr
.global process_trap_vector_size
process_spawn_addr: .dword process_spawn
process_spawn_size: .dword process_spawn_end - process_spawn
process_trap_vector_addr: .dword process_trap_vector
process_trap_vector_size: .dword process_trap_vector_end - process_trap_vector
