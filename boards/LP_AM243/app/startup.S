/* SPDX-License-Identifier: Apache-2.0
 *
 * startup.S - minimal Cortex-R5F reset for the AM243x-LP test application.
 *
 * The application is a RAW binary linked to run from MSRAM at BOOT_LOAD_ADDR
 * (see linker.ld). Its own vector table is first, so execution can start at the
 * image base: the boot verifier sets the released R5F's
 * PC to BOOT_LOAD_ADDR and the reset vector below takes it from there.
 */
    .arm
    .section .vectors, "ax"
    .global _vectors
_vectors:
    ldr pc, =Reset_Handler      /* 0x00 reset            */
    ldr pc, =_hang              /* 0x04 undefined        */
    ldr pc, =_hang              /* 0x08 svc              */
    ldr pc, =_hang              /* 0x0C prefetch abort   */
    ldr pc, =_hang              /* 0x10 data abort       */
    nop                         /* 0x14 reserved         */
    ldr pc, =_hang              /* 0x18 irq              */
    ldr pc, =_hang              /* 0x1C fiq              */

    .text
    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    cpsid  if                   /* mask IRQ + FIQ                        */
    mrs    r0, cpsr
    bic    r0, r0, #0x1F
    orr    r0, r0, #0x13        /* force SVC mode, ARM state             */
    msr    cpsr_c, r0
    ldr    sp, =_stack_top

    /* zero .bss */
    ldr    r0, =_sbss
    ldr    r1, =_ebss
    mov    r2, #0
1:  cmp    r0, r1
    bhs    2f
    str    r2, [r0], #4
    b      1b
2:
    bl     main

    .global _hang
    .type _hang, %function
_hang:
    b      _hang
    .size  _hang, . - _hang
