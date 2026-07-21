.section .text.boot
.global _start
.type _start, %function

_start:
    // QEMU and the Pi firmware pass the DTB pointer in x0. Keep it in a
    // callee-saved register across the privilege transition and early setup.
    mov x19, x0

    // The kernel owns one early stack and is not SMP-ready. Firmware may make
    // secondary cores visible, so only affinity-0 enters shared initialization.
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cbnz x1, .Lsecondary_halt

    // No asynchronous exception may arrive before the EL1 stack and vector
    // table exist.
    msr daifset, #0xf

    mrs x1, CurrentEL
    cmp x1, #0x8
    b.eq .Lfrom_el2
    cmp x1, #0x4
    b.eq .Lel1_entry
    b .Lprimary_halt

.Lfrom_el2:
    // Run EL1 and EL0 as AArch64. Permit EL1 access to the physical timer and
    // start with stage-2 translation disabled.
    mov x1, #(1 << 31)
    msr hcr_el2, x1
    mrs x1, cnthctl_el2
    orr x1, x1, #0x3
    msr cnthctl_el2, x1
    msr cntvoff_el2, xzr
    msr vttbr_el2, xzr

    // The normal bootstrap constructs EL1 translation state deliberately.
    // Preserve architectural RES1 state while disabling inherited EL1 MMU and
    // cache enable bits.
    mrs x1, sctlr_el1
    bic x1, x1, #0x1
    bic x1, x1, #0x4
    bic x1, x1, #0x1000
    msr sctlr_el1, x1

    // Return to EL1h with debug, SError, IRQ, and FIQ masked. The first EL1
    // instructions install SP_EL1 before any call or exception can use it.
    mov x1, #0x3c5
    msr spsr_el2, x1
    adr x1, .Lel1_entry
    msr elr_el2, x1
    isb
    eret

.Lel1_entry:
    // Install the early kernel stack after reaching the execution level used
    // by every exception vector and C subsystem.
    adrp x0, __stack_top
    add x0, x0, :lo12:__stack_top
    mov sp, x0

    // Set VBAR_EL1 while the boot path is still single-threaded.
    bl exception_init

    // Clear zero-initialized storage before kernel_main touches globals.
    adrp x0, __bss_start
    add x0, x0, :lo12:__bss_start
    adrp x1, __bss_end
    add x1, x1, :lo12:__bss_end
    bl zero_bss

    mov x0, x19
    bl kernel_main

.Lprimary_halt:
    wfe
    b .Lprimary_halt

.Lsecondary_halt:
    wfe
    b .Lsecondary_halt

.type zero_bss, %function
zero_bss:
    cmp x0, x1
    b.hs .Lzero_done

.Lzero_loop:
    str xzr, [x0], #8
    cmp x0, x1
    b.lo .Lzero_loop

.Lzero_done:
    ret
