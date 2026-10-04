/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * enclave.c - AM243x M4F crypto enclave (DECISIONS.md -> D3, "inverted
 * architecture", LOCKED).
 *
 * Request/response loop that carries no state between calls: the R5F (`boot`)
 * fills enclave_req_t at ENCLAVE_REQ_ADDR and signals us over IPC Notify; we
 * compute into
 * enclave_resp_t at ENCLAVE_RESP_ADDR and signal back. We never decide
 * pass/fail for the boot flow - the R5F reads our verdict and enforces it.
 *
 * Two ops, both always built:
 *   ENCLAVE_OP_SHA256: pqm4's own mupq/common/sha2.c (R1, as-is) - not a
 *     bespoke common/sha256.c; this project already proved that swap out on
 *     the M4F-orchestrator design this replaces, and it stands on its own
 *     merits here too (a real upstream-vendored implementation over a
 *     hand-rolled one).
 *   ENCLAVE_OP_VERIFY: pqm4 ML-DSA m4f verify, as-is (R1).
 *
 * The M4F has no cache (CacheP.h: "M4: Not supported"), so reading the
 * R5F-written request needs no cache maintenance on this side - the R5F side
 * (boot/main.c) is the one that must invalidate before reading our response.
 */
#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/CycleCounterP.h>   /* M4F DWT CYCCNT */
#include <drivers/ipc_notify.h>

#include "enclave.h"
#include "sha2.h"          /* pqm4 mupq/common (R1, as-is): sha256ctx, sha256_inc_* */
#include "api.h"           /* pqm4 ML-DSA m4f: crypto_sign_verify, CRYPTO_BYTES */
#include "enclave_ipc.h"   /* ../common/ */
#include "manifest.h"      /* <repo>/common/ : PQSB_HDR_LEN */
#include "stack_paint.h"   /* ../common/ : stack high-water measurement */

/*
 * SHA-256 over the 32-byte header, then image[0:img_len] - the two byte-ranges
 * the host signer covers (the signature + 0xFF padding between them are
 * skipped by the caller never asking us to hash them). Same block0-merge
 * technique proven in the earlier M4F-orchestrator design: sha256_inc_blocks()
 * only accepts whole 64-byte blocks, so the header can't be absorbed alone -
 * merge it with the head of the image into one block, absorb that, then
 * finalize over whatever of the image is left (finalize() hashes any full
 * blocks in its input, then pads the tail per FIPS 180-4).
 */
static void enclave_sha256(const uint8_t *hdr, uint32_t hdr_len,
                            const uint8_t *img, uint32_t img_len,
                            uint8_t out[32])
{
    sha256ctx c;
    uint8_t   block0[64];
    uint32_t  take;

    sha256_inc_init(&c);

    if (hdr_len + img_len <= sizeof(block0)) {
        memcpy(block0, hdr, hdr_len);
        memcpy(block0 + hdr_len, img, img_len);
        sha256_inc_finalize(out, &c, block0, hdr_len + img_len);
        return;
    }

    take = (uint32_t)sizeof(block0) - hdr_len;   /* bytes of img that fill block 0 */
    memcpy(block0, hdr, hdr_len);
    memcpy(block0 + hdr_len, img, take);
    sha256_inc_blocks(&c, block0, 1);
    sha256_inc_finalize(out, &c, img + take, img_len - take);
}

static volatile uint32_t gRequestPending = 0;

static void reqCallback(uint32_t remoteCoreId, uint16_t localClientId,
                         uint32_t msgValue, int32_t crcStatus, void *args)
{
    (void)remoteCoreId;
    (void)localClientId;
    (void)msgValue;
    (void)crcStatus;
    (void)args;
    gRequestPending = 1;
}

#if PQSB_STACK_PAINT
extern uint32_t __STACK_END;    /* high address; MSP grows DOWN from it */
extern uint32_t __STACK_SIZE;   /* a linker VALUE - take its address    */

#define MSP_STACK_END   ((uint32_t *)&__STACK_END)
#define MSP_STACK_BASE  ((uint32_t *)((uintptr_t)&__STACK_END - (uintptr_t)&__STACK_SIZE))

void enclave_stackPaint(void)
{
    uint32_t *sp;

    __asm__ volatile ("mov %0, sp" : "=r" (sp));
    pqsb_stack_paint(MSP_STACK_BASE, sp - 16);
}
#endif

static void handleRequest(void)
{
    const enclave_req_t *rq  = (const enclave_req_t *)ENCLAVE_REQ_ADDR;
    enclave_resp_t       *rs = (enclave_resp_t *)ENCLAVE_RESP_ADDR;

    rs->seq    = rq->seq;
    rs->status = ENCLAVE_STATUS_OK;

    CycleCounterP_reset();

    if (rq->op == ENCLAVE_OP_SHA256) {
        const uint8_t *hdr = (const uint8_t *)(uintptr_t)rq->header_addr;
        const uint8_t *img = (const uint8_t *)(uintptr_t)rq->image_addr;
        uint32_t t0 = CycleCounterP_getCount32();
        enclave_sha256(hdr, rq->header_len, img, rq->image_len, rs->digest);
        rs->compute_cycles = CycleCounterP_getCount32() - t0;
    } else if (rq->op == ENCLAVE_OP_VERIFY) {
        const uint8_t *sig = (const uint8_t *)(uintptr_t)rq->sig_addr;
        const uint8_t *pk  = (const uint8_t *)(uintptr_t)rq->pk_addr;
        uint32_t t0 = CycleCounterP_getCount32();
        int rc = crypto_sign_verify(sig, rq->sig_len, rq->digest, 32, pk);  /* empty ctx */
        rs->compute_cycles = CycleCounterP_getCount32() - t0;
        rs->verdict = (rc == 0) ? 1u : 0u;
        if (rc != 0) {
            rs->status = ENCLAVE_STATUS_FAIL;
        }
    } else {
        rs->status = ENCLAVE_STATUS_FAIL;
    }

#if PQSB_STACK_PAINT
    /* Read after the op, so the mark covers SHA/verify and everything under
     * them - including the IPC callback, which shares this same MSP. */
    rs->stack_peak = pqsb_stack_peak(MSP_STACK_BASE, MSP_STACK_END);
#else
    rs->stack_peak = 0u;
#endif

    IpcNotify_sendMsg(CSL_CORE_ID_R5FSS0_0, ENCLAVE_IPC_CLIENT_RESP, rs->seq, 1);
}

void enclave_main(void *args)
{
    (void)args;

    DebugP_log("\r\n==== AM243 enclave (M4F crypto service) ====\r\n");
    DebugP_log("enclave: SHA256 + VERIFY\r\n");

    IpcNotify_registerClient(ENCLAVE_IPC_CLIENT_REQ, reqCallback, NULL);

    for (;;) {
        while (!gRequestPending) {
            /* spin - woken by reqCallback() from ISR context */
        }
        gRequestPending = 0;
        handleRequest();
    }
}
