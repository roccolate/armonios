.section .text

/*
 * irq_disable()/irq_enable() are used by code that also runs from exception
 * handlers. A plain enable on the way out is unsafe there: EL0 SVC and IRQ
 * handlers enter with I masked, so a nested helper must restore that masked
 * state rather than unconditionally unmasking the timer.
 */
.section .bss
.align 3
irq_disable_depth:
    .word 0
irq_restore_enabled:
    .word 0

.section .text

// IRQ mask helpers.
//
// DAIF bit #2 is the AArch64 IRQ mask. These helpers are intentionally tiny so
// C code can bracket scheduler-critical regions without learning DAIF encoding
// details. The ISB makes the mask change visible before the caller continues.
.global irq_enable
.type irq_enable, %function

irq_enable:
    ldr x1, =irq_disable_depth
    ldr w2, [x1]
    cbz w2, .Lirq_enable_force

    sub w2, w2, #1
    str w2, [x1]
    cbnz w2, .Lirq_enable_done

    ldr x1, =irq_restore_enabled
    ldr w2, [x1]
    cbz w2, .Lirq_enable_done

    msr daifclr, #2
    isb
    ret

.Lirq_enable_force:
    msr daifclr, #2
    isb

.Lirq_enable_done:
    ret

.global irq_disable
.type irq_disable, %function

irq_disable:
    ldr x1, =irq_disable_depth
    ldr w2, [x1]
    cbnz w2, .Lirq_disable_nested

    mrs x3, daif
    ubfx w2, w3, #7, #1
    eor w2, w2, #1
    ldr x1, =irq_restore_enabled
    str w2, [x1]

    ldr x1, =irq_disable_depth
    mov w2, #1
    str w2, [x1]
    msr daifset, #2
    isb
    ret

.Lirq_disable_nested:
    add w2, w2, #1
    str w2, [x1]
    ret

/*
 * The vector always saves a complete exception_frame_t, but only an IRQ that
 * interrupted EL0 owns a schedulable user trap frame. An IRQ taken while EL1
 * is executing a syscall or another kernel path must service the device and
 * return to that exact kernel context; passing its frame to irq_handler_frame
 * would let process preemption overwrite EL1 registers and switch TTBR0.
 *
 * x0: exception_frame_t * captured by exception_vectors.S
 */
.global irq_handler_frame_from_vector
.type irq_handler_frame_from_vector, %function

irq_handler_frame_from_vector:
    cbz x0, .Lirq_origin_dispatch
    ldr x1, [x0, #256]
    and x1, x1, #0x0f
    cbz x1, .Lirq_origin_dispatch
    mov x0, xzr

.Lirq_origin_dispatch:
    b irq_handler_frame
