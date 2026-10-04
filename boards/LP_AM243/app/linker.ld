/* SPDX-License-Identifier: Apache-2.0
 *
 * linker.ld - AM243x-LP test app: one raw image linked to run from MSRAM at
 * BOOT_LOAD_ADDR. Vector table first, so entry == image base (BOOT_ENTRY).
 *
 * MSRAM 0x70000000..0x70200000 (2 MiB). `boot`'s own generated memory regions
 * (example.syscfg) run through 0x70070000, and a small APPIMAGE scratch region
 * sits at 0x701BC000..0x701C0000; the top 128 KiB (0x701E0000+) is
 * DMSC-reserved. The one gap that's free unconditionally is
 * 0x70070000..0x701BC000 - see common/boot_handoff.h for the full accounting
 * (it also covers why the top-of-MSRAM DMSC region isn't usable here). Of that
 * gap, the bottom 0x1000 bytes are spoken for (the enclave IPC mailboxes, plus
 * the 32 bytes just below BOOT_LOAD_ADDR where boot stages the header for
 * hashing), so this region takes everything from BOOT_LOAD_ADDR up to
 * APPIMAGE: 0x70071000..0x701BC000, 1324 KiB. That is the largest bootable
 * image on this board, and it is bounded by RAM, not by flash.
 *
 * LENGTH must stay equal to boot/main.c's BOOT_LOAD_MAX - `boot` refuses to
 * stage an image larger than that, and reads it straight to ORIGIN.
 * tools/provisioning.py --pad-to grows the *signed image* (raw bytes appended
 * after this ELF's own content, outside anything the linker tracks) for the
 * image-size benchmark; padding is hashed and verified but never executed, and
 * _stack_top below sits at the top of the region, so a nearly-full image has
 * the stack descending through padding - harmless, since verification has
 * already completed before the jump.
 * Keep this ORIGIN in sync with common/boot_handoff.h's BOOT_LOAD_ADDR /
 * BOOT_ENTRY - a mismatch here is a silent data abort on real hardware (this
 * app is a position-dependent raw binary: _stack_top/_sbss/_ebss are baked
 * in as absolute addresses at link time, so running it from anywhere but
 * where it was linked for corrupts memory on the very first instructions of
 * Reset_Handler, before it can print anything).
 */
ENTRY(_vectors)

MEMORY
{
    MSRAM (rwx) : ORIGIN = 0x70071000, LENGTH = 0x14B000  /* 1324 KiB */
}

SECTIONS
{
    . = ORIGIN(MSRAM);

    .vectors : ALIGN(32)
    {
        KEEP(*(.vectors))
    } > MSRAM

    .text : ALIGN(4)
    {
        *(.text*)
        *(.rodata*)
        *(.glue_7) *(.glue_7t)
    } > MSRAM

    .data : ALIGN(4)
    {
        *(.data*)
    } > MSRAM

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = ALIGN(4);
    } > MSRAM

    . = ALIGN(8);
    _end = .;

    _stack_top = ORIGIN(MSRAM) + LENGTH(MSRAM);

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
