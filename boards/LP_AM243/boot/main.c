/*
 * boot/main.c - R5F orchestrator for the AM243x-LP PQC (ML-DSA) secure boot.
 *
 * SPDX-License-Identifier: BSD-3-Clause AND Apache-2.0
 *
 * Derived from Texas Instruments' MCU+ SDK example
 *   examples/drivers/boot/sbl_ospi/am243x-lp/r5fss0-0_nortos/main.c
 *   (MCU+ SDK 12.00.00.27)
 * The SBL skeleton in main() - SYSFW and board bring-up, loading and starting
 * the M4F from the multicore appimage - together with gAppimage and
 * loop_forever(), is TI's code, distributed under TI's BSD-3-Clause notice
 * reproduced unchanged below.
 *
 * Modified for this project: main() no longer resets itself into ATCM after
 * loading the other cores, and instead runs boot_run() and hands off to the
 * verified application. Everything else in this file - the M4F enclave IPC
 * client, the OSPI read path, the ML-DSA verification flow, the boot report
 * and the jump to the application - is new, and is
 *   Copyright 2026 Liviu Silaghe, licensed under Apache-2.0 (see LICENSE).
 */

/*
 *  Copyright (C) 2018-2026 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/sciclient.h>
#include <drivers/soc.h>                       /* SOC_getSelfCpuClk */
#include <drivers/bootloader/soc/bootloader_soc.h>
#include <drivers/bootloader.h>
#include <drivers/ipc_notify.h>
#include <board/flash.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/CycleCounterP.h>
#include <kernel/dpl/HwiP.h>                   /* HwiP_disable/restore (config B) */
/* Signed-image format: repo-root common/, because the host signer and the
 * other board implement the same header independently. Everything below it is
 * AM243x-specific and lives in this board's own common/. */
#include "manifest.h"           /* <repo>/common/ : pqsb_boot_hdr_t, PQSB_*, sig/pk lengths */
#include "enclave_ipc.h"        /* ../common/ : the R5F<->M4F crypto-enclave IPC contract */
#include "boot_handoff.h"       /* ../common/ : BOOT_LOAD_ADDR / BOOT_ENTRY */
#include "boot_report.h"        /* ../common/ : boot_report_t for the launched application */
#include "stack_paint.h"        /* ../common/ : stack high-water measurement (STACK_PAINT=1) */

/*
 * `boot` (R5F, DECISIONS.md -> D3 "inverted architecture", LOCKED) drives the
 * whole flow: reads signed_app.bin from OSPI, obtains a SHA-256 digest and an
 * ML-DSA verdict, enforces the verdict itself, and on PASS jumps straight to
 * the verified image at BOOT_LOAD_ADDR. No IPC "go" signal is needed for that
 * last step (unlike the earlier M4F-orchestrator design this replaces), since
 * the R5F already holds the verdict and does its own hand-off in the same core.
 *
 * Two configurations, selected at build time (DECISIONS.md -> D4):
 *
 *   BOOT_CONFIG_A - both crypto steps run on the M4F `enclave` (a crypto
 *       service that holds no state between requests), each requested over IPC:
 *       software SHA-256, then ML-DSA verify in pqm4's hand-written Cortex-M4
 *       assembly. This R5F links no crypto at all.
 *
 *   BOOT_CONFIG_B - both steps run here, in software, and the enclave is not
 *       used at all: the multicore ELF is never even parsed, so the M4F is
 *       never started and nothing is ever sent or received over IPC. SHA-256 is
 *       the same pqm4 mupq/common/sha2.c the enclave runs in A; ML-DSA verify
 *       is PQClean's portable-C "clean" implementation, because pqm4's is M4
 *       assembly and cannot run on an R5F.
 *
 *       (One syscfg serves both configurations, so Drivers_open() still brings
 *       the IPC Notify driver up in B. Nothing then uses it - no client is
 *       registered and no message is sent - and it happens long before the
 *       cycle counter is reset, so it is outside every timed window.)
 *
 * So A vs B asks one question: what does it cost to keep post-quantum
 * verification on the orchestrator instead of handing it to a second core
 * across an IPC boundary?
 */
#ifndef BOOT_IMAGE_BASE
#define BOOT_IMAGE_BASE   0x60300000u   /* signed_app.bin slot   */
#endif
#ifndef BOOT_PUBKEY_BASE
#define BOOT_PUBKEY_BASE  0x60200000u   /* standalone pubkey.bin */
#endif

/* This buffer needs to be defined for OSPI boot in case of HS device for
 * image decryption and authentication
 * Incase of am243x-lp only 256kb is available in RAM, so the encrypted image should be
 * less than 256kb.
 */
/*
 * TI's encrypted-boot scratch buffer, kept as the SDK example declares it.
 * Nothing here references it - the only use is under `#ifdef ENC_BOOT`, which
 * this project does not define - so the linker drops it and the APPIMAGE region
 * reads back fully unused in both configurations' maps.
 *
 * The section name must stay ".bss.app": that is what SysConfig's generated
 * linker.cmd binds to the APPIMAGE region. A section it does not know about is
 * not an error - the linker just auto-places it in the first region it fits,
 * which at this 16 KiB size is R5F TCMA at 0x1000, core-local and invisible to
 * any DMA master. Harmless while unreferenced, a silent trap the moment
 * anything does use it.
 */
uint8_t gAppimage[0x4000] __attribute__ ((section (".bss.app"), aligned (4096)));

/* call this API to stop the booting process and spin, do that you can connect
 * debugger, load symbols and then make the 'loop' variable as 0 to continue execution
 * with debugger connected.
 */
void loop_forever(void)
{
    volatile uint32_t loop = 1;
    while(loop)
        ;
}

/* ---- enclave IPC client: bidirectional request/response ---------------
 * Config A only. Config B never talks to the enclave, so none of this is even
 * compiled in - the IPC cost it is being compared against has to be absent,
 * not merely unused. */
#if !BOOT_CONFIG_B
static volatile uint32_t gRespReady = 0;

static void respCallback(uint32_t remoteCoreId, uint16_t localClientId,
                          uint32_t msgValue, int32_t crcStatus, void *args)
{
    (void)remoteCoreId;
    (void)localClientId;
    (void)msgValue;
    (void)crcStatus;
    (void)args;
    gRespReady = 1;
}

/*
 * Fill *rq (seq auto-assigned), notify the enclave, wait for its response,
 * copy it into *rsOut. `boot` writes the request from a different bus master
 * than the M4F reads it from, and reads the response the M4F wrote the same
 * way - this R5F's own MPU config marks that MSRAM region cacheable (unlike
 * the M4F, which has no cache at all - CacheP.h: "M4: Not supported"), so a
 * writeback after filling the request (make sure the M4F sees fresh bytes,
 * not whatever was in a not-yet-flushed R5F cache line) and an invalidate
 * before reading the response (make sure we fetch what the M4F actually
 * wrote, not stale cached data) are both required. Same class of bug as the
 * app-image hand-off cache issue found and fixed in the earlier design.
 */
static uint32_t gSeq = 0;

static void enclaveCall(enclave_req_t *rq, enclave_resp_t *rsOut)
{
    enclave_req_t  *shReq  = (enclave_req_t *)ENCLAVE_REQ_ADDR;
    enclave_resp_t *shResp = (enclave_resp_t *)ENCLAVE_RESP_ADDR;

    rq->seq = ++gSeq;
    memcpy(shReq, rq, sizeof(*shReq));
    CacheP_wb((void *)shReq, sizeof(*shReq), CacheP_TYPE_L1D);

    gRespReady = 0;
    IpcNotify_sendMsg(CSL_CORE_ID_M4FSS0_0, ENCLAVE_IPC_CLIENT_REQ, rq->seq, 1);

    while (!gRespReady) {
        /* spin - woken by respCallback() from ISR context */
    }

    CacheP_inv((void *)shResp, sizeof(*shResp), CacheP_TYPE_L1D);
    memcpy(rsOut, shResp, sizeof(*rsOut));
}
#endif /* !BOOT_CONFIG_B */

#if PQSB_STACK_PAINT
/*
 * Stack high-water measurement (build with STACK_PAINT=1). The R5F has six
 * stacks, one per processor mode, and which one C code runs on is not obvious:
 * boot_armv7r_asm.S initialises each in turn and finishes in SYSTEM mode with
 * sp = __STACK_END, so main() and everything it calls runs on .stack - NOT
 * .svcstack. (Getting this wrong cost a whole debugging session once PQClean's
 * verify started running on this core: it overflowed the then 16 KiB .stack
 * silently into the heap and .bss. That is config B's ~92 KiB peak, against
 * ~1.4 KiB in config A, so the reservation is sized for B and the measurement
 * is what justifies it.)
 *
 * All five mode stacks are measured, not just the one main() runs on, because
 * the generated linker.cmd's own comment about where ISRs run is wrong for
 * this build: it says the switch to SVC mode for user ISR callbacks is a
 * FreeRTOS behaviour and that in NoRTOS "this is the stack used by ISRs
 * registered as type IRQ". The first measurement disproved that - .irqstack
 * came back at 8 B with the IPC callback demonstrably running. Measure all of
 * them and let the numbers say which is which. .abortstack/.undefinedstack
 * are free diagnostics too: anything but 0 there means a fault was taken.
 */
extern uint32_t __STACK_END;        /* high address; .stack grows DOWN from it */
extern uint32_t __STACK_SIZE;       /* a linker VALUE - take its address       */
extern uint32_t __IRQ_STACK_START,       __IRQ_STACK_END;
extern uint32_t __FIQ_STACK_START,       __FIQ_STACK_END;
extern uint32_t __SVC_STACK_START,       __SVC_STACK_END;
extern uint32_t __ABORT_STACK_START,     __ABORT_STACK_END;
extern uint32_t __UNDEFINED_STACK_START, __UNDEFINED_STACK_END;

#define SYS_STACK_END   ((uint32_t *)&__STACK_END)
#define SYS_STACK_BASE  ((uint32_t *)((uintptr_t)&__STACK_END - (uintptr_t)&__STACK_SIZE))

static const struct {
    const char *name;
    uint32_t   *base;
    uint32_t   *end;
} gModeStacks[] = {
    { "irqstack",   &__IRQ_STACK_START,       &__IRQ_STACK_END       },
    { "fiqstack",   &__FIQ_STACK_START,       &__FIQ_STACK_END       },
    { "svcstack",   &__SVC_STACK_START,       &__SVC_STACK_END       },
    { "abortstack", &__ABORT_STACK_START,     &__ABORT_STACK_END     },
    { "undefstack", &__UNDEFINED_STACK_START, &__UNDEFINED_STACK_END },
};
#define NUM_MODE_STACKS  (sizeof(gModeStacks) / sizeof(gModeStacks[0]))

static void paintStacks(void)
{
    uint32_t *sp;
    unsigned  i;

    /* Our own frame and everything the caller already pushed live above sp;
     * stop a little short of it so painting does not scribble on them. */
    __asm__ volatile ("mov %0, sp" : "=r" (sp));
    if ((sp > SYS_STACK_END) || (sp < SYS_STACK_BASE)) {
        /* Not standing on .stack after all - paint it whole rather than write
         * somewhere live. The peak then reads 0 B, which is the tell. */
        sp = SYS_STACK_END + 16;
    }
    pqsb_stack_paint(SYS_STACK_BASE, sp - 16);

    /* We are in SYSTEM mode and no interrupt is registered yet, so every other
     * mode's stack is idle and can be painted end to end. */
    for (i = 0; i < NUM_MODE_STACKS; i++) {
        pqsb_stack_paint(gModeStacks[i].base, gModeStacks[i].end);
    }
}

static void reportStacks(void)
{
    unsigned i;

    DebugP_log("[mem] r5f .stack     : %6u B used of %6u B reserved (SYSTEM mode - main())\r\n",
               (unsigned)pqsb_stack_peak(SYS_STACK_BASE, SYS_STACK_END),
               (unsigned)(uintptr_t)&__STACK_SIZE);

    for (i = 0; i < NUM_MODE_STACKS; i++) {
        DebugP_log("[mem] r5f .%-10s: %6u B used of %6u B reserved\r\n",
                   gModeStacks[i].name,
                   (unsigned)pqsb_stack_peak(gModeStacks[i].base, gModeStacks[i].end),
                   (unsigned)((uintptr_t)gModeStacks[i].end -
                              (uintptr_t)gModeStacks[i].base));
    }
}
#endif /* PQSB_STACK_PAINT */

/* Plain branch-and-exchange to the verified image's own reset vector
 * (boards/LP_AM243/app/startup.S: `ldr pc, =Reset_Handler` at offset 0) - not
 * a C call, so no link register / return is involved, matching a real boot
 * hand-off. `bx` reads the address' bit 0 to select ARM vs Thumb state;
 * BOOT_ENTRY is word-aligned (bit 0 clear), matching the app's `.arm` vector
 * table. The app's own Reset_Handler masks IRQ/FIQ and sets up its own
 * SVC-mode stack, so no register state beyond PC needs to be prepared here.
 * `dsb`/`isb` first: this R5F just wrote the image bytes itself (boot_run(),
 * below) into the same cacheable MSRAM region it's about to fetch
 * instructions from - the barriers make sure that write has actually landed
 * before the jump, not just sit in a store buffer. */
static void jumpToApp(uint32_t entry)
{
    __asm__ volatile ("dsb\n\tisb\n\tbx %0" : : "r" (entry) :);
}

/*
 * Reading this flash correctly is narrower than it looks, and getting it wrong
 * is silent. The OSPI runs 4S-4D-4D (DDR) with phyEnable, and in the driver's
 * read path (ospi_v0_lld.c, OSPI_lld_readDirect) the PHY *pipeline* is only
 * switched on for the DMA leg. Anything the driver hands to its CPU fallback -
 * OSPI_lld_flashMemcpy - is fetched with the pipeline off and comes back with
 * each 32-bit word duplicated across the 8-byte DDR beat. Measured on target:
 * a 32-byte read of the header returned magic, magic, image_size, image_size
 * instead of magic, 1, image_size, algo_id, while a 4 KiB read of the same
 * offset was byte-perfect.
 *
 * So a read only returns correct data if it takes the DMA leg end to end. From
 * that same function, all four must hold:
 *   - destination is not a DMA-restricted region (gOspiDmaRestrictRegions in
 *     the generated config: R5F ATCM and M4F IRAM/DRAM; MSRAM is fine),
 *   - count > OSPI_DMA_COPY_LOWER_LIMIT (1024),
 *   - flash offset is 32-byte aligned  (else the head bytes go to the CPU leg),
 *   - count is a multiple of 32        (else the tail bytes go to the CPU leg).
 * Every flashRead() call below satisfies all four; keep it that way when adding
 * one, and round sizes up rather than reading exactly what is needed.
 *
 * Dereferencing the memory-mapped XIP window (0x60000000+) directly is worse
 * still - it is not a supported read path at all and produces the same
 * duplicated-word garbage. `volatile` does not help: this is the peripheral,
 * not the compiler. A JTAG dump of the same address looks perfect, because the
 * debug probe does not read through this path - so "the flash content is fine"
 * proves nothing about what the CPU can get.
 *
 * Finally, do NOT add cache maintenance around Flash_read. The driver owns it
 * for whichever leg it picks; invalidating the destination afterwards throws
 * away bytes the CPU leg just stored, which reads back as all zeros. The
 * writeback the enclave needs is a separate concern - see boot_run().
 */
#define OSPI_XIP_BASE     0x60000000u
#define BOOT_IMAGE_OFF    (BOOT_IMAGE_BASE  - OSPI_XIP_BASE)
#define BOOT_PUBKEY_OFF   (BOOT_PUBKEY_BASE - OSPI_XIP_BASE)

/* Smallest transfer that still takes the DMA leg, and the alignment every
 * transfer must be rounded to. */
#define FLASH_DMA_MIN     1056u
#define FLASH_DMA_ALIGN   32u

/* MSRAM reserved for the application: BOOT_LOAD_ADDR (0x70071000) up to the
 * top of the SBL-free gap (0x701BC000, where the APPIMAGE scratch region
 * begins). This is a RAM ceiling, not a flash one - the image runs from MSRAM.
 * Keep in sync with app/linker.ld's MSRAM LENGTH; see ../common/boot_handoff.h. */
#ifndef BOOT_LOAD_MAX
#define BOOT_LOAD_MAX     0x14B000u  /* 1324 KB */
#endif

/* Staging buffers, also read by the enclave over IPC. gSlot takes the whole
 * 8 KiB header region in one transfer, so the header and the signature that
 * follows it both arrive on the DMA leg; gPk is padded well past the largest
 * ML-DSA public key for the same reason. */
static uint8_t gSlot[PQSB_RESERVE]  __attribute__((aligned(FLASH_DMA_ALIGN)));
static uint8_t gPk[4096]            __attribute__((aligned(FLASH_DMA_ALIGN)));

/*
 * Stand-in for the device's root-of-trust fuse.
 *
 * On an HS-SE AM243x the customer key hash lives in eFuses and is enforced by
 * ROM/TIFS before the SBL ever runs. This board is HS-FS: those fuses are not
 * programmed, so there is nothing real to compare against and the verdict
 * below is advisory only - it is computed, reported and deliberately NOT acted
 * on. The point is to carry the measurement, so the benchmark reports what a
 * complete chain of trust costs rather than what a shortcut costs.
 *
 * Reading the real fuses from here would not work well anyway, and the reason
 * is worth recording: TISCI exposes TISCI_MSG_READ_OTP_MMR, but it returns ONE
 * 32-bit row per call - eight round trips for a digest. A real HS-SE port would
 * take the verdict from ROM rather than re-deriving it here.
 *
 * Deliberately a value that cannot match, so a reader of the log is never
 * misled into thinking an unprogrammed part passed a real check.
 */
static const uint8_t gRotFuseDigest[32] = {
    0xDE, 0xAD, 0xF0, 0x5E, 0xDE, 0xAD, 0xF0, 0x5E,
    0xDE, 0xAD, 0xF0, 0x5E, 0xDE, 0xAD, 0xF0, 0x5E,
    0xDE, 0xAD, 0xF0, 0x5E, 0xDE, 0xAD, 0xF0, 0x5E,
    0xDE, 0xAD, 0xF0, 0x5E, 0xDE, 0xAD, 0xF0, 0x5E
};

/* Constant-time-ish compare. The digest of a public key is not secret, so this
 * is habit rather than necessity - but a root-of-trust comparison is exactly
 * the place where the habit should hold. */
static bool rotDigestEqual(const uint8_t *a, const uint8_t *b)
{
    uint8_t diff = 0U;
    for (uint32_t i = 0U; i < 32U; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return (diff == 0U);
}

static int32_t flashRead(uint32_t offset, void *dst, uint32_t len)
{
    return Flash_read(gFlashHandle[CONFIG_FLASH0], offset, (uint8_t *)dst, len);
}

#if BOOT_CONFIG_B
#include "sha2.h"   /* pqm4 mupq/common (R1, as-is) */

/*
 * Config B: software SHA-256 on this R5F, using the same pqm4 sha2.c the M4F
 * enclave runs in config A - same source, same -O2. So for the hash step A vs B
 * isolates "which core, plus the IPC round trip" from "which SHA-256
 * implementation": only the core differs. (The verify step cannot be held equal
 * this way - see verify_on_r5f() below.)
 *
 * Same block0-merge as the enclave: sha256_inc_blocks() takes whole 64-byte
 * blocks only, so the 32-byte header cannot be absorbed on its own. Merge it
 * with the head of the image into one block, absorb that, then finalize over
 * the rest of the image.
 */
static void sha256_two_ranges_sw(const uint8_t *hdr, uint32_t hdrLen,
                                 const uint8_t *img, uint32_t imgLen,
                                 uint8_t out[32])
{
    sha256ctx c;
    uint8_t   block0[64];
    uint32_t  take;

    sha256_inc_init(&c);

    if (hdrLen + imgLen <= sizeof(block0)) {
        memcpy(block0, hdr, hdrLen);
        memcpy(block0 + hdrLen, img, imgLen);
        sha256_inc_finalize(out, &c, block0, hdrLen + imgLen);
        return;
    }

    take = (uint32_t)sizeof(block0) - hdrLen;
    memcpy(block0, hdr, hdrLen);
    memcpy(block0 + hdrLen, img, take);
    sha256_inc_blocks(&c, block0, 1);
    sha256_inc_finalize(out, &c, img + take, imgLen - take);
}

/*
 * Config B: ML-DSA verify also runs here on the R5F, using PQClean's portable-C
 * "clean" implementation. pqm4's m4f code cannot be used - it is hand-written
 * Cortex-M4 assembly - so this is a different implementation of the same
 * algorithm. That is the honest way to read the verify half of A vs B: it
 * measures "verify on the R5F in portable C" against "verify on the M4F in
 * tuned assembly", not one core against the other with everything else equal.
 * State it that way in any write-up; the hash half above has no such caveat.
 *
 * Only ONE parameter set is linked, chosen at build time by PQC_SCHEME: the
 * three clean implementations together overflow MSRAM. That is the one way
 * config B differs from A, which picks the scheme from the image header at
 * runtime. The macro maps to the ctx variant with NULL/0, i.e. the empty
 * context this project signs with - see DECISIONS.md.
 */
#if PQC_SCHEME == 44
#include "../../../third_party_sw/pqm4/mupq/pqclean/crypto_sign/ml-dsa-44/clean/api.h"
#define PQC_VERIFY      PQCLEAN_MLDSA44_CLEAN_crypto_sign_verify
#define PQC_ALGO_ID     PQSB_ALGO_ML_DSA_44
#elif PQC_SCHEME == 65
#include "../../../third_party_sw/pqm4/mupq/pqclean/crypto_sign/ml-dsa-65/clean/api.h"
#define PQC_VERIFY      PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify
#define PQC_ALGO_ID     PQSB_ALGO_ML_DSA_65
#else
#include "../../../third_party_sw/pqm4/mupq/pqclean/crypto_sign/ml-dsa-87/clean/api.h"
#define PQC_VERIFY      PQCLEAN_MLDSA87_CLEAN_crypto_sign_verify
#define PQC_ALGO_ID     PQSB_ALGO_ML_DSA_87
#endif

static bool verify_on_r5f(uint32_t algoId, const uint8_t *sig, uint32_t sigLen,
                          const uint8_t *digest, const uint8_t *pk)
{
    /* Built for one parameter set only, so say so plainly instead of failing
     * deep inside verify on a length check. */
    if (algoId != PQC_ALGO_ID) {
        DebugP_log("boot: image is algo_id %u but this config B build links "
                   "ML-DSA-%u - rebuild with PQC_SCHEME=<44|65|87>\r\n",
                   (unsigned)algoId, (unsigned)PQC_SCHEME);
        return false;
    }
    return (PQC_VERIFY(sig, sigLen, digest, 32, pk) == 0);
}
#endif /* BOOT_CONFIG_B */


/*
 * Reads the signed image off OSPI into MSRAM (the image straight to its
 * execution address, BOOT_LOAD_ADDR), gets its digest and ML-DSA verdict from
 * the enclave over IPC (config A: both SHA256 and VERIFY requests), and
 * enforces the verdict. Writes boot_report_t either way. True iff authentic.
 */
static bool boot_run(void)
{
    pqsb_boot_hdr_t h;

    /* One transfer for the whole header region: the header at [0] and the
     * signature at [PQSB_HDR_LEN] both land on the DMA leg. */
    if (flashRead(BOOT_IMAGE_OFF, gSlot, PQSB_RESERVE) != SystemP_SUCCESS) {
        DebugP_log("boot: header flash read failed\r\n");
        return false;
    }
    memcpy(&h, gSlot, sizeof(h));

    if (h.magic != PQSB_MAGIC) {
        DebugP_log("boot: bad magic (0x%08x)\r\n", (unsigned)h.magic);
        return false;
    }
    if (h.hdr_version != PQSB_HDR_VERSION) {
        DebugP_log("boot: bad header version (%u)\r\n", (unsigned)h.hdr_version);
        return false;
    }

    /* Scheme-agnostic by construction: sig/pk lengths come from the image's
     * own header via the shared manifest.h table, not a build-time
     * PQSB_SCHEME - `boot` never links pqm4 (only `enclave` does), so it has
     * no scheme of its own to cross-check against. A scheme `enclave` wasn't
     * built for still fails - just inside the enclave's own crypto_sign_verify
     * call (wrong CRYPTO_BYTES/CRYPTO_PUBLICKEYBYTES baked into that build),
     * reported back as an ordinary VERIFY FAIL. */
    uint32_t sig_len, pk_len;
    switch (h.algo_id) {
        case PQSB_ALGO_ML_DSA_44: sig_len = PQSB_SIG_LEN_44; pk_len = PQSB_PUBKEY_LEN_44; break;
        case PQSB_ALGO_ML_DSA_65: sig_len = PQSB_SIG_LEN_65; pk_len = PQSB_PUBKEY_LEN_65; break;
        case PQSB_ALGO_ML_DSA_87: sig_len = PQSB_SIG_LEN_87; pk_len = PQSB_PUBKEY_LEN_87; break;
        default:
            DebugP_log("boot: bad algo_id (%u)\r\n", (unsigned)h.algo_id);
            return false;
    }

    uint32_t len = h.image_size;

    DebugP_log("image_size: %u\r\n", (unsigned)len);

    /* image_size is attacker-controlled until VERIFY passes, and it is about to
     * be used as a read length into a fixed-size region - bound it first. */
    if (len == 0u || len > BOOT_LOAD_MAX) {
        DebugP_log("boot: image_size out of range (%u, max %u)\r\n",
                   (unsigned)len, (unsigned)BOOT_LOAD_MAX);
        return false;
    }

    /* Round the image transfer up to keep it on the DMA leg. Reading a little
     * past the image is harmless - the slack lands inside the app's own region,
     * which it initialises itself - but only the declared len is ever hashed. */
    uint32_t readLen = (len + (FLASH_DMA_ALIGN - 1u)) & ~(FLASH_DMA_ALIGN - 1u);
    if (readLen < FLASH_DMA_MIN) {
        readLen = FLASH_DMA_MIN;
    }
    if (readLen > BOOT_LOAD_MAX) {
        DebugP_log("boot: image too large to stage (%u)\r\n", (unsigned)readLen);
        return false;
    }

    /* The image is read straight to its final execution address, so the bytes
     * that get hashed and verified are exactly the bytes that get executed -
     * no second read of the flash afterwards that could return something else. */
    if (flashRead(BOOT_PUBKEY_OFF, gPk, sizeof(gPk)) != SystemP_SUCCESS ||
        flashRead(BOOT_IMAGE_OFF + PQSB_RESERVE, (void *)BOOT_LOAD_ADDR, readLen) != SystemP_SUCCESS) {
        DebugP_log("boot: flash read failed\r\n");
        return false;
    }

    /* Everything above may still be sitting dirty in this R5F's D-cache.
     * Config A hands the enclave addresses into these buffers and the M4F has
     * no cache at all (CacheP.h: "M4: Not supported"), so it reads MSRAM
     * directly and needs them flushed. Config B reads them with its own loads
     * and would not strictly need this - but it runs here too, deliberately and
     * unconditionally: both configurations then enter the timed window from the
     * same cache state, which is what makes their figures comparable. It is
     * outside the window either way (CycleCounterP_reset() is below). */
    CacheP_wbInvAll(CacheP_TYPE_ALL);

#if !BOOT_CONFIG_B
    enclave_req_t  rq;              /* config B never talks to the enclave */
    enclave_resp_t rs;
    uint32_t verify_m4f;            /* enclave's own DWT count, M4F clock domain */
    uint32_t sha_m4f;               /* config A hashes there too... */
    uint32_t rot_m4f;               /* ...including the RoT key hash */
#endif
    uint32_t t0, sha_cycles, verify_cycles;
    uint8_t  digest[32];
    uint8_t  pkDigest[32];
    uint32_t rot_cycles;
    bool     rotMatch;

    CycleCounterP_reset();

    /* --- stage 0: root of trust - bind the public key to the device ------
     *
     * Until this runs, gPk is trusted for no better reason than that it was in
     * flash, and flash is exactly what an attacker rewrites. The chain has to
     * terminate in something immutable: on an HS-SE part that is the customer
     * key hash burned into eFuses, and the key is only usable if SHA-256 over
     * it matches. Without this step the whole ML-DSA verification proves only
     * that the image matches *some* key, which is no property at all.
     *
     * Hashed by the SAME implementation on the SAME core this configuration
     * uses for the image, so the cost lands where the configuration says it
     * should - and it doubles as a small-input data point (2,592 B against
     * ~1.3 MB), where per-call fixed overhead is no longer amortised. For
     * config A that fixed overhead is a whole second IPC round trip, which is
     * exactly what makes the contrast worth printing.
     *
     * Measured but NOT enforced. See gRotFuseDigest for why. */
#if BOOT_CONFIG_B
    /* stage 0 (see above): RoT key binding, same software SHA-256 as the image */
    t0 = CycleCounterP_getCount32();
    sha256_two_ranges_sw(gPk, 0U, gPk, pk_len, pkDigest);
    rotMatch   = rotDigestEqual(pkDigest, gRotFuseDigest);
    rot_cycles = CycleCounterP_getCount32() - t0;

    /* config B: software SHA-256 on this core; there is no enclave to ask */
    t0 = CycleCounterP_getCount32();
    sha256_two_ranges_sw(gSlot, PQSB_HDR_LEN,
                         (const uint8_t *)BOOT_LOAD_ADDR, len, digest);
    sha_cycles = CycleCounterP_getCount32() - t0;
#else
    /* stage 0 (see above): RoT key binding, on the same engine as the image -
     * for config A that means a second IPC round trip, which is exactly the
     * cost this configuration's architecture implies. header_len = 0 makes the
     * enclave hash one range; its block0-merge handles that case already. */
    memset(&rq, 0, sizeof(rq));
    rq.op          = ENCLAVE_OP_SHA256;
    rq.header_addr = (uint32_t)(uintptr_t)gPk;
    rq.header_len  = 0U;
    rq.image_addr  = (uint32_t)(uintptr_t)gPk;
    rq.image_len   = pk_len;
    t0 = CycleCounterP_getCount32();
    enclaveCall(&rq, &rs);
    rotMatch   = rotDigestEqual(rs.digest, gRotFuseDigest);
    rot_cycles = CycleCounterP_getCount32() - t0;
    rot_m4f    = rs.compute_cycles;   /* the enclave's own DWT count for this hash */
    if (rs.status != ENCLAVE_STATUS_OK) {
        DebugP_log("boot: enclave pubkey hash failed\r\n");
        return false;
    }
    memcpy(pkDigest, rs.digest, 32);

    /* config A: requested from the M4F enclave over IPC */
    memset(&rq, 0, sizeof(rq));
    rq.op          = ENCLAVE_OP_SHA256;
    rq.header_addr = (uint32_t)(uintptr_t)gSlot;
    rq.header_len  = PQSB_HDR_LEN;
    rq.image_addr  = (uint32_t)BOOT_LOAD_ADDR;
    rq.image_len   = len;
    t0 = CycleCounterP_getCount32();
    enclaveCall(&rq, &rs);
    sha_cycles = CycleCounterP_getCount32() - t0;
    sha_m4f    = rs.compute_cycles;

    if (rs.status != ENCLAVE_STATUS_OK) {
        DebugP_log("boot: enclave SHA256 op failed\r\n");
        return false;
    }
    memcpy(digest, rs.digest, 32);
#endif

    DebugP_log("rot: pubkey sha256 = ");
    for (int i = 0; i < 32; i++) {
        DebugP_log("%02x", pkDigest[i]);
    }
    DebugP_log("\r\n");
    /* Never gates the boot: this part's fuses hold no reference to compare
     * against, so acting on the verdict would be theatre. Says so plainly. */
    DebugP_log("rot: fuse compare %s - ADVISORY ONLY (HS-FS part, "
               "key-hash fuses not programmed)\r\n",
               rotMatch ? "match" : "MISMATCH");

    DebugP_log("SHA_256 digest: ");
    for (int i = 0; i < 32; i++) {
        DebugP_log("%02x", digest[i]);
    }
    DebugP_log("\r\n");

    /* --- stage 2: ML-DSA verify --- */
#if BOOT_CONFIG_B
    /*
     * Mask interrupts across the verify so the figure carries no jitter.
     * Nothing is expected to fire: this is a single-threaded SBL, the console
     * is polled (intrEnable=DISABLE in example.syscfg), and config B registers
     * no IPC client and starts no second core - so this is cheap insurance
     * rather than a fix for anything observed.
     */
    uintptr_t irqKey = HwiP_disable();

    /* config B: verified here on the R5F in portable C; no enclave, no IPC */
    t0 = CycleCounterP_getCount32();
    bool ok = verify_on_r5f(h.algo_id, gSlot + PQSB_HDR_LEN, sig_len, digest, gPk);
    verify_cycles = CycleCounterP_getCount32() - t0;
    HwiP_restore(irqKey);
#else
    memset(&rq, 0, sizeof(rq));
    rq.op = ENCLAVE_OP_VERIFY;
    memcpy(rq.digest, digest, 32);
    rq.sig_addr = (uint32_t)(uintptr_t)(gSlot + PQSB_HDR_LEN);
    rq.sig_len  = sig_len;
    rq.pk_addr  = (uint32_t)(uintptr_t)gPk;
    rq.pk_len   = pk_len;
    t0 = CycleCounterP_getCount32();
    enclaveCall(&rq, &rs);
    verify_cycles = CycleCounterP_getCount32() - t0;
    verify_m4f    = rs.compute_cycles;

    bool ok = (rs.status == ENCLAVE_STATUS_OK) && (rs.verdict == 1u);
#endif

    uint32_t cpu_hz = (uint32_t)SOC_getSelfCpuClk();
    uint32_t per_us = (cpu_hz >= 1000000u) ? (cpu_hz / 1000000u) : 1u;
#if !BOOT_CONFIG_B
    /* Config A spans two clock domains: the round trip is counted on this R5F,
     * the compute on the M4F's own DWT. The M4F clock is queried with the same
     * device / clock pair the bootloader used to set it (gCoreBootInfo in
     * bootloader_soc.c), not assumed. Config B never starts the M4F, so it does
     * not ask - one core, one clock, nothing to rescale. */
    uint64_t m4f_hz64 = 0;
    Sciclient_pmGetModuleClkFreq(TISCI_DEV_MCU_M4FSS0_CORE0,
                                 TISCI_DEV_MCU_M4FSS0_CORE0_VBUS_CLK,
                                 &m4f_hz64, SystemP_WAIT_FOREVER);
    uint32_t m4f_hz     = (uint32_t)m4f_hz64;
    uint32_t m4f_per_us = (m4f_hz >= 1000000u) ? (m4f_hz / 1000000u) : 1u;
#endif

#if BOOT_CONFIG_B
    DebugP_log("[bench] R5F @ %u MHz  (config B: SW SHA256 + PQClean ML-DSA verify, both on R5F, no IPC)\r\n",
               (unsigned)(cpu_hz / 1000000u));
    DebugP_log("[bench] sha256 (r5f sw): %9u R5F cycles  (%u us)\r\n",
               (unsigned)sha_cycles, (unsigned)(sha_cycles / per_us));
#else
    DebugP_log("[bench] R5F @ %u MHz, M4F @ %u MHz  (config A: SHA256+VERIFY via IPC)\r\n",
               (unsigned)(cpu_hz / 1000000u), (unsigned)(m4f_hz / 1000000u));
    DebugP_log("[bench] sha256 (ipc)  : %10u R5F cycles  (%u us)\r\n",
               (unsigned)sha_cycles, (unsigned)(sha_cycles / per_us));
    DebugP_log("[bench] sha256 (m4f)  : %10u M4F cycles  (%u us)\r\n",
               (unsigned)sha_m4f, (unsigned)(sha_m4f / m4f_per_us));
#endif
    /* Printed next to the image hash on purpose: same implementation, same
     * units, two input sizes three orders of magnitude apart. Over ~1.3 MB the
     * per-call fixed cost is invisible; over 2.5 KB it dominates - and in
     * config A that fixed cost is an entire IPC round trip. Reported by both
     * configs so the RoT step can be added to or taken out of either total
     * consistently. */
#if BOOT_CONFIG_B
    DebugP_log("[bench] rot pk hash   : %10u R5F cycles  (%u us)  over %u B\r\n",
               (unsigned)rot_cycles, (unsigned)(rot_cycles / per_us),
               (unsigned)pk_len);
#else
    /* Config A hashes the key on the enclave, so the same two-clock-domain split
     * the image hash and verify get applies here too: round trip on this R5F,
     * pure compute on the M4F's own DWT. */
    DebugP_log("[bench] rot pk hash (ipc): %10u R5F cycles  (%u us)  over %u B\r\n",
               (unsigned)rot_cycles, (unsigned)(rot_cycles / per_us),
               (unsigned)pk_len);
    DebugP_log("[bench] rot pk hash (m4f): %10u M4F cycles  (%u us)\r\n",
               (unsigned)rot_m4f, (unsigned)(rot_m4f / m4f_per_us));
#endif
#if BOOT_CONFIG_B
    DebugP_log("[bench] verify (r5f C): %10u R5F cycles  (%u us)\r\n",
               (unsigned)verify_cycles, (unsigned)(verify_cycles / per_us));
#else
    DebugP_log("[bench] verify (ipc)  : %10u R5F cycles  (%u us)\r\n",
               (unsigned)verify_cycles, (unsigned)(verify_cycles / per_us));
    DebugP_log("[bench] verify (m4f)  : %10u M4F cycles  (%u us)\r\n",
               (unsigned)verify_m4f, (unsigned)(verify_m4f / m4f_per_us));
#endif

#if !BOOT_CONFIG_B
    /* IPC overhead = round trip minus the enclave's own compute, done in R5F
     * cycles (M4F count rescaled by the clock ratio) so neither side is
     * rounded to whole microseconds before subtracting. Config B has no IPC
     * boundary to account for - that absence is the point of the comparison,
     * so there is deliberately no line here rather than a zero. */
    if (m4f_hz != 0u) {
        int32_t ver_ovh = (int32_t)(verify_cycles -
            (uint32_t)(((uint64_t)verify_m4f * cpu_hz) / m4f_hz));
        int32_t sha_ovh = (int32_t)(sha_cycles -
            (uint32_t)(((uint64_t)sha_m4f * cpu_hz) / m4f_hz));
        DebugP_log("[bench] ipc overhead  : sha256 %d R5F cycles (%d us), verify %d R5F cycles (%d us)\r\n",
                   (int)sha_ovh, (int)(sha_ovh / (int32_t)per_us),
                   (int)ver_ovh, (int)(ver_ovh / (int32_t)per_us));
    }
#endif /* !BOOT_CONFIG_B */

#if PQSB_STACK_PAINT
    /* Read the marks here, after the deepest work (hash + verify) is done but
     * before the hand-off memcpy, which is shallow and would only add noise. */
    reportStacks();
#if !BOOT_CONFIG_B
    DebugP_log("[mem] m4f stack peak : %6u B used (enclave .stack, reported over IPC)\r\n",
               (unsigned)rs.stack_peak);
#endif
#endif

    DebugP_log(ok ? "VERIFY PASS\r\n" : "VERIFY FAIL\r\n");

    boot_report_t *rep = (boot_report_t *)BOOT_REPORT_ADDR;
    rep->magic         = BOOT_REPORT_MAGIC;
    rep->version       = BOOT_REPORT_VERSION;
#if BOOT_CONFIG_B
    rep->config        = BOOT_REPORT_CONFIG_B;
#else
    rep->config        = BOOT_REPORT_CONFIG_A;
#endif
    rep->verify_pass   = ok ? 1u : 0u;
    rep->algo_id       = h.algo_id;
    rep->image_size    = len;
    rep->sha_cycles    = sha_cycles;
    rep->verify_cycles = verify_cycles;
    rep->cpu_hz        = cpu_hz;

    if (ok) {
        DebugP_log("boot: image authentic\r\n");
        DebugP_log("boot: image at 0x%08x (%u bytes)\r\n",
                   (unsigned)BOOT_LOAD_ADDR, (unsigned)len);
    } else {
        DebugP_log("boot: authentication FAILED\r\n");
    }

    return ok;
}

int main(void)
{
    int32_t status;

#if PQSB_STACK_PAINT
    /* First statement in main(): the earlier this runs, the smaller the
     * unpaintable floor (see ../common/stack_paint.h). Nothing before this point
     * has done more than set up the mode stacks in boot_armv7r_asm.S. */
    paintStacks();
#endif

    Bootloader_profileReset();

    Bootloader_socWaitForFWBoot();

#ifndef DISABLE_WARM_REST_WA
    /* Warm Reset Workaround to prevent CPSW register lockup */
    if (!Bootloader_socIsMCUResetIsoEnabled())
    {
        Bootloader_socResetWorkaround();
    }
#endif

    Bootloader_profileAddProfilePoint("SYSFW init");

    if (!Bootloader_socIsMCUResetIsoEnabled())
    {
        /* Update devGrp to ALL to initialize MCU domain when reset isolation is
        not enabled */
        Sciclient_BoardCfgPrms_t boardCfgPrms_pm =
            {
                .boardConfigLow = (uint32_t)0,
                .boardConfigHigh = 0,
                .boardConfigSize = 0,
                .devGrp = DEVGRP_ALL,
            };

        status = Sciclient_boardCfgPm(&boardCfgPrms_pm);

        Sciclient_BoardCfgPrms_t boardCfgPrms_rm =
        {
            .boardConfigLow = (uint32_t)0,
            .boardConfigHigh = 0,
            .boardConfigSize = 0,
            .devGrp = DEVGRP_ALL,
        };

        status = Sciclient_boardCfgRm(&boardCfgPrms_rm);

        /* Enable MCU PLL. MCU PLL will not be enabled by DMSC when devGrp is set
        to Main in boardCfg */
        Bootloader_enableMCUPLL();
    }

    System_init();
    Bootloader_profileAddProfilePoint("System_init");

    Bootloader_socOpenFirewalls();

    Bootloader_socNotifyFirewallOpen();


    Drivers_open();
    Bootloader_profileAddProfilePoint("Drivers_open");

#if !BOOT_CONFIG_B
    /* Register before releasing the M4F enclave, so its response can never
     * arrive before we're listening for it. */
    status = IpcNotify_registerClient(ENCLAVE_IPC_CLIENT_RESP, respCallback, NULL);
    DebugP_assert(status == SystemP_SUCCESS);
#endif

    #if 0
    DebugP_log("\r\n");
    DebugP_log("Starting OSPI Bootloader ... \r\n");
    #endif

    status = Board_driversOpen();
    DebugP_assert(status == SystemP_SUCCESS);
    Bootloader_profileAddProfilePoint("Board_driversOpen");

    status = Sciclient_getVersionCheck(1);
    Bootloader_profileAddProfilePoint("Sciclient Get Version");
    if(SystemP_SUCCESS == status)
    {
#if !BOOT_CONFIG_B
        Bootloader_BootImageInfo bootImageInfo;
#endif
        Bootloader_Params bootParams;
        Bootloader_Handle bootHandle;
        #ifdef ENC_BOOT
        Bootloader_Config *bootConfig;
        #endif

        Bootloader_Params_init(&bootParams);
#if !BOOT_CONFIG_B
        Bootloader_BootImageInfo_init(&bootImageInfo);
#endif

        bootHandle = Bootloader_open(CONFIG_BOOTLOADER_FLASH0, &bootParams);
        if(bootHandle != NULL)
        {
            /* Initialize PRU Cores if applicable */
            Bootloader_Config *cfg = (Bootloader_Config *)bootHandle;
            #ifdef ENC_BOOT
            bootConfig = (Bootloader_Config *)bootHandle;
            bootConfig->scratchMemPtr = gAppimage;
            #else
            cfg->enableScratchMem = 0U;
            #endif

            if(TRUE == cfg->initICSSCores)
            {
                status = Bootloader_socEnableICSSCores(BOOTLOADER_ICSS_CORE_DEFAULT_FREQUENCY);
                DebugP_assert(status == SystemP_SUCCESS);
            }

#if BOOT_CONFIG_B
            /*
             * Config B hashes AND verifies entirely on this R5F - there is no
             * enclave call anywhere in the boot path - so the M4F image is
             * never loaded and never started. Skipping the multicore ELF here
             * is what makes that real rather than nominal: it drops the
             * bootloader's ELF parser and, with it, the 56 KiB gElfBuffer that
             * dominates .bss in config A. The enclave image does not need to be
             * flashed at 0x80000 for a B build at all, which is also why
             * scripts/flash.bat takes a --code-only switch.
             */
#else
            status = Bootloader_parseAndLoadMultiCoreELF(bootHandle, &bootImageInfo);

            Bootloader_profileAddProfilePoint("CPU load");
            Bootloader_profileUpdateAppimageSize(Bootloader_getMulticoreImageSize(bootHandle));
#endif
            Bootloader_profileUpdateMediaAndClk(BOOTLOADER_MEDIA_FLASH, OSPI_getInputClk(gOspiHandle[CONFIG_OSPI0]));

            #if 1
            if( status == SystemP_SUCCESS)
            {
                /* Enable Phy and Phy pipeline for XIP execution */
                if( OSPI_isPhyEnable(gOspiHandle[CONFIG_OSPI0]) )
                {
                    status = OSPI_enablePhy(gOspiHandle[CONFIG_OSPI0]);
                    DebugP_assert(status == SystemP_SUCCESS);

                    status = OSPI_enablePhyPipeline(gOspiHandle[CONFIG_OSPI0]);
                    DebugP_assert(status == SystemP_SUCCESS);
                }
                /* Enable Dac mode */
                status = OSPI_enableDacMode(gOspiHandle[CONFIG_OSPI0]);
                DebugP_assert(status == SystemP_SUCCESS);
            }
            #endif

            if(status == SystemP_SUCCESS)
            {
                /* Print SBL Profiling logs to UART as other cores may use the UART for logging */
                Bootloader_profileAddProfilePoint("SBL End");
                Bootloader_profilePrintProfileLog();
#if BOOT_CONFIG_B
                DebugP_log("No enclave in config B - verifying on this core ...\r\n");
#else
                DebugP_log("Image loading done, switching to application ...\r\n");
#endif
                UART_flushTxFifo(gUartHandle[CONFIG_UART0]);
            }

#if !BOOT_CONFIG_B
            /* Run CPUs */
            /* Do not run M4 when MCU domain is reset isolated */
            if (!Bootloader_socIsMCUResetIsoEnabled())
            {
                if(status == SystemP_SUCCESS && (TRUE == Bootloader_isCorePresent(bootHandle, CSL_CORE_ID_M4FSS0_0)))
                {
                    status = Bootloader_runCpu(bootHandle, &bootImageInfo.cpuInfo[CSL_CORE_ID_M4FSS0_0]);
                }
            }
            if(status == SystemP_SUCCESS && (TRUE == Bootloader_isCorePresent(bootHandle, CSL_CORE_ID_R5FSS1_0)))
            {
                status = Bootloader_runCpu(bootHandle, &bootImageInfo.cpuInfo[CSL_CORE_ID_R5FSS1_0]);
            }
            /*Checks the core variant(Dual/Quad) */
            if((Bootloader_socIsR5FSSDual(BOOTLOADER_R5FSS1)) && status == SystemP_SUCCESS && (TRUE == Bootloader_isCorePresent(bootHandle, CSL_CORE_ID_R5FSS1_1)))
            {
                status = Bootloader_runCpu(bootHandle, &bootImageInfo.cpuInfo[CSL_CORE_ID_R5FSS1_1]);
            }
#endif /* !BOOT_CONFIG_B */
            if(status == SystemP_SUCCESS)
            {
                /* Do NOT reset self (Bootloader_runSelfCpu()) - that would jump
                 * R5F0-0 into whatever is in its own ATCM, which for a bundle
                 * with no R5F0-0 image is just a reset vector + WFI, i.e.
                 * silently go to sleep and never run the orchestration logic
                 * below. Instead stay on this core and run boot_run(), which
                 * drives the verify either through the enclave (config A) or
                 * locally (config B). */
                DebugP_log("\r\n==== AM243 boot (R5F orchestrator) ====\r\n");
                bool ok = boot_run();
                UART_flushTxFifo(gUartHandle[CONFIG_UART0]);

                if (ok) {
                    DebugP_log("jumping to 0x%08x\r\n", (unsigned)BOOT_ENTRY);
                    UART_flushTxFifo(gUartHandle[CONFIG_UART0]);
                    /* This R5F just wrote the image bytes itself (boot_run())
                     * into the same cacheable MSRAM region it's about to fetch
                     * instructions from - invalidate before the jump so the
                     * fetch below sees what was actually written, not any
                     * stale cache state. */
                    CacheP_wbInvAll(CacheP_TYPE_L1P | CacheP_TYPE_L1D);
                    jumpToApp(BOOT_ENTRY);
                    /* does not return */
                } else {
                    DebugP_log("boot: halting\r\n");
                    UART_flushTxFifo(gUartHandle[CONFIG_UART0]);
                    loop_forever();
                }
            }
            /* it should not return here, if it does, then there was some error */
            Bootloader_close(bootHandle);
        }
    }
    if(status != SystemP_SUCCESS )
    {
        DebugP_log("Some tests have failed!!\r\n");
    }
    Drivers_close();
    System_deinit();

    return 0;
}
