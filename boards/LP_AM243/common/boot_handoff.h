/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * boot_handoff.h - the verified-image execution address, shared by `boot`
 * (R5F, DECISIONS.md -> D3 "inverted architecture") and the launched
 * application's own linker script (boards/LP_AM243/app/linker.ld).
 *
 * Post-D3-inversion, `boot` (R5F) owns the whole verify decision itself (it
 * gets a verdict back from the `enclave` over enclave_ipc.h, not a
 * one-shot "jump now" signal) - so only the address constants below are still
 * load-bearing. `boot`, on a PASS verdict, copies the verified bytes here and
 * jumps to BOOT_ENTRY directly in the same core, no IPC signal needed for
 * that step.
 */
#ifndef PQSB_BOOT_HANDOFF_H
#define PQSB_BOOT_HANDOFF_H

/*
 * MSRAM execution region the verified image is copied to. AM243x has 2 MB
 * MSRAM (0x70000000-0x70200000, see docs/api_guide_am243x/MEMORY_MAP.html).
 * The top 128 KB (0x701E0000+) is DMSC-reserved, and the 80 KB of it that
 * normally frees up after the SBL's self-release security handover does NOT
 * apply here - `boot` deliberately skips that self-reset (see boot/main.c), so
 * that hand-off event never fires. Current map:
 *
 *   0x70000000 .. 0x70000100   MSRAM_VECS
 *   0x70000100 .. 0x70030000   MSRAM_0    192 KB  .text/.rodata/.data
 *   0x70030000 .. 0x70070000   MSRAM_1    256 KB  .stack/.bss/mode stacks/heap
 *   0x70070000 .. 0x70070200   ENCLAVE_REQ_ADDR / ENCLAVE_RESP_ADDR
 *   0x70070200 .. 0x70071000   free
 *   0x70071000 .. 0x701BC000   THE APPLICATION      0x14B000 = 1324 KB
 *   0x701BC000 .. 0x701C0000   APPIMAGE   16 KB  SBL scratch (unused here)
 *   0x701C0000 ..              BOOT_REPORT_ADDR (boot_report.h)
 *
 * 1324 KB is the hard ceiling on a bootable image, and it is a RAM limit, not a
 * flash one - the image executes from MSRAM, while the OSPI part is 64 MB.
 * boot/main.c's BOOT_LOAD_MAX and app/linker.ld's MSRAM ORIGIN+LENGTH encode
 * these numbers and must change together with them; an app built for the old
 * base will not run, since it is a position-dependent raw binary.
 *
 * It was 896 KB (0x700A0000..0x70180000) until the stack painting measurements
 * showed where the slack really was. Two reclaims, both measured rather than
 * guessed: MSRAM_1 384 -> 256 KB (peak .stack use is 92 856 B in config B and
 * 1 432 B in config A, against 192 KB reserved), and APPIMAGE 256 -> 16 KB
 * (nothing in this project uses that scratch region). BOOT_REPORT_ADDR used to sit
 * at 0x70170000, inside the window, which is why the ceiling was once 128 KB.
 *
 * The application is a raw binary linked to run from MSRAM (locked - see
 * DECISIONS.md -> "P3 design"); entry at BOOT_ENTRY (default = BOOT_LOAD_ADDR,
 * no separate load/entry metadata in the <8I> header).
 */
#ifndef BOOT_LOAD_ADDR
#define BOOT_LOAD_ADDR   0x70071000u
#endif
#ifndef BOOT_ENTRY
#define BOOT_ENTRY       BOOT_LOAD_ADDR
#endif

#endif /* PQSB_BOOT_HANDOFF_H */
