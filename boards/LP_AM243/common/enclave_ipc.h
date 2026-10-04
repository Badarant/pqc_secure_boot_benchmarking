/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * enclave_ipc.h - R5F -> M4F crypto-enclave IPC contract (shared).
 *
 * Inverted architecture (DECISIONS.md -> D3, LOCKED): the R5F (`boot`) drives
 * the boot flow and calls the M4F (`enclave`) as a crypto service that CARRIES
 * NO STATE between requests. (Deliberately not called "stateless": in PQC that
 * word classifies signature schemes - FIPS 205 SLH-DSA is a Stateless Hash-Based
 * DSA, LMS/XMSS are stateful - and this is a statement about the service, not
 * about ML-DSA.)
 * The R5F fills a request block, signals the M4F (IPC Notify), the M4F reads
 * the referenced bytes from shared MSRAM, computes,
 * writes the response block, and signals back.
 *
 * Ops:
 *   ENCLAVE_OP_SHA256: hash header||image -> digest.
 *   ENCLAVE_OP_VERIFY: ML-DSA verify(sig, digest, pk) -> verdict.
 * Used by `boot`'s config A. Config B does both steps on the R5F and never
 * starts this core, so none of this contract is exercised there.
 *
 * Pointers are SoC-global MSRAM addresses both cores can read - never OSPI XIP
 * (0x60000000+). The R5F stages every byte the enclave reads (header,
 * signature, public key, image) into MSRAM first, because CPU reads through the
 * XIP window return corrupted data under this board's OSPI config (see
 * boards/LP_AM243/boot/main.c, above flashRead()). The R5F also writes its
 * D-cache back before signalling, since the M4F has no cache and reads MSRAM
 * directly.
 *
 * The enclave is app-agnostic (R3) and key-agnostic: the pubkey travels in the
 * VERIFY request (the R5F owns pubkey.bin).
 */
#ifndef PQSB_ENCLAVE_IPC_H
#define PQSB_ENCLAVE_IPC_H

#include <stdint.h>

/*
 * AM243x has 2 MB MSRAM (0x70000000-0x70200000, see
 * docs/api_guide_am243x/MEMORY_MAP.html). `boot`'s (R5F) own generated
 * memory regions run through 0x70070000, and a small APPIMAGE scratch
 * region sits at 0x701BC000-0x701C0000; the top 128 KB (0x701E0000+) is
 * DMSC-reserved, and the 80 KB of it that normally frees up after the SBL's
 * self-release security handover does NOT apply here - this `boot` never
 * self-resets (see boot/main.c). So these live in the one gap that's free
 * unconditionally: [0x70070000, 0x701BC000) - see boot_handoff.h for
 * the full accounting and what else shares that gap (BOOT_LOAD_ADDR,
 * BOOT_REPORT_ADDR).
 */
#ifndef ENCLAVE_REQ_ADDR
#define ENCLAVE_REQ_ADDR   0x70070000u
#endif
#ifndef ENCLAVE_RESP_ADDR
#define ENCLAVE_RESP_ADDR  0x70070100u
#endif

#define ENCLAVE_OP_SHA256  1u
#define ENCLAVE_OP_VERIFY  2u

#define ENCLAVE_STATUS_OK      0u
#define ENCLAVE_STATUS_FAIL    1u   /* verify invalid / op error */

/* IPC Notify client IDs for the request/response signal pair. Client IDs 0/1
 * are reserved by the SDK (rpmsg / sync); pick our own above that range -
 * distinct from PQSB_IPC_CLIENT_HANDOFF (boot_handoff.h), which is a
 * different, one-shot signal (R5F -> R5F's own released application, not
 * this bidirectional enclave protocol). */
#define ENCLAVE_IPC_CLIENT_REQ   3u   /* R5F -> M4F: request ready */
#define ENCLAVE_IPC_CLIENT_RESP  4u   /* M4F -> R5F: response ready */

typedef struct {
    uint32_t op;            /* ENCLAVE_OP_* */
    uint32_t seq;           /* request id, echoed in the response */
    /* SHA256 inputs (op == SHA256): hash header then image */
    uint32_t header_addr;   uint32_t header_len;   /* header_len == 32 */
    uint32_t image_addr;    uint32_t image_len;
    /* VERIFY inputs (op == VERIFY) */
    uint8_t  digest[32];                            /* message = SHA256(hdr||img) */
    uint32_t sig_addr;      uint32_t sig_len;       /* = scheme CRYPTO_BYTES */
    uint32_t pk_addr;       uint32_t pk_len;        /* = scheme CRYPTO_PUBLICKEYBYTES */
} enclave_req_t;

typedef struct {
    uint32_t seq;           /* echoes the request */
    uint32_t status;        /* ENCLAVE_STATUS_* */
    uint8_t  digest[32];    /* SHA256 result (op == SHA256) */
    uint32_t verdict;       /* 1 = signature valid, 0 = invalid (op == VERIFY) */
    uint32_t compute_cycles;/* M4F DWT cycles for the op (pure compute) */
    uint32_t stack_peak;    /* M4F .stack high-water in bytes, 0 unless both
                             * sides are built with STACK_PAINT=1. The M4F owns
                             * this number - only it can see its own stack - so
                             * it rides back here rather than being printed on
                             * the enclave's console, which has no cable. */
} enclave_resp_t;

#endif /* PQSB_ENCLAVE_IPC_H */
