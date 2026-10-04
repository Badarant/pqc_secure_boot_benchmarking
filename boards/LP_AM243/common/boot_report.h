/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * boot_report.h - boot -> application hand-off report (shared contract).
 *
 * `boot` (R5F, DECISIONS.md -> D3 "inverted architecture") measures its own
 * wall-clock cost for the hash and the verify step - in config A a SHA256
 * request and a VERIFY request, each including the full round-trip to the M4F
 * `enclave` and back; in config B its own local software compute for both -
 * with its own cycle counter,
 * writes this struct to a fixed MSRAM address, and the launched application
 * reads it and prints it on its UART. It lives in common/ so neither `boot`
 * nor the application references the other (R2/R3): `boot` writes an opaque
 * report block, the application consumes it.
 */
#ifndef PQSB_BOOT_REPORT_H
#define PQSB_BOOT_REPORT_H

#include <stdint.h>

/* Fixed MSRAM address of the report. Deliberately NOT inside the
 * [0x70090000, 0x70180000) gap that BOOT_LOAD_ADDR lives in: this used to sit
 * at 0x70170000 and capped the application at 128 KB, because a report placed
 * mid-gap is a wall the image cannot grow past. It now sits in the otherwise
 * unused 128 KB between the APPIMAGE scratch region (ends 0x701C0000) and the
 * DMSC-reserved top of MSRAM (starts 0x701E0000), which leaves the whole gap
 * contiguous for the image - see boot_handoff.h.
 *
 * Still not the DMSC region itself: the 80 KB that frees up there normally
 * does so only after the SBL's self-release security handover, which `boot`'s
 * custom flow (it never self-resets - see boot/main.c) never triggers. */
#ifndef BOOT_REPORT_ADDR
#define BOOT_REPORT_ADDR   0x701C0000u
#endif

#define BOOT_REPORT_MAGIC   0x54505242u   /* 'BRPT' */
#define BOOT_REPORT_VERSION 3u            /* v3: R5F-orchestrated (D3); added config */

#define BOOT_REPORT_CONFIG_A  0x41u  /* 'A': SW SHA-256 + ML-DSA verify, both on the M4F enclave over IPC */
#define BOOT_REPORT_CONFIG_B  0x42u  /* 'B': SW SHA-256 + ML-DSA verify, both on the R5F, no IPC */

typedef struct {
    uint32_t magic;          /* BOOT_REPORT_MAGIC   */
    uint32_t version;        /* BOOT_REPORT_VERSION */
    uint32_t config;         /* BOOT_REPORT_CONFIG_A/B */
    uint32_t verify_pass;    /* 1 = authentic, 0 = failed */
    uint32_t algo_id;        /* 1/2/3 = ML-DSA-44/65/87 */
    uint32_t image_size;     /* image bytes hashed */
    uint32_t sha_cycles;     /* R5F cycles for the hash step:
                              *   config A: enclave SHA256 IPC round-trip
                              *             (incl. the M4F's compute)
                              *   config B: the R5F's own software compute */
    uint32_t verify_cycles;  /* R5F cycles for the ML-DSA verify step:
                              *   config A: enclave VERIFY IPC round-trip
                              *             (incl. the M4F's compute)
                              *   config B: the R5F's own software compute */
    uint32_t cpu_hz;         /* R5F clock (Hz) the cycle counts above were
                              * measured at - not the M4F's clock, since
                              * `boot` (R5F) does the round-trip timing now. */
} boot_report_t;

#endif /* PQSB_BOOT_REPORT_H */
