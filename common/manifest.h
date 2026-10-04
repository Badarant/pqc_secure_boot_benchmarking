/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * manifest.h - PQC secure-boot header, matched byte-for-byte to the existing
 *              Nucleo host signer (tools/py_dilithium_sign/secureboot-host-sign_app.py).
 *              Shared by the host verify companion and the device verifiers
 *              (Nucleo L4R5 and AM243 M4F). Nucleo is untouched; AM243 conforms.
 *
 * Why this file sits in the REPO ROOT common/ while its neighbours do not: it is
 * the only genuinely cross-cutting contract here. Three implementations of this
 * one layout exist and must agree -
 *   - this struct, included by boards/LP_AM243/{boot,enclave};
 *   - boards/nucleo_L4R5ZI/boot/main.c, which re-declares the same magic and
 *     field order by hand and reads it with index arithmetic;
 *   - tools/py_dilithium_sign/secureboot-host-sign_app.py, which PRODUCES it
 *     from its own Python copy of the constants.
 * Change the layout here and all three have to change together; there is no
 * build-time coupling that will catch it for you.
 *
 * Everything that is specific to one board lives under that board instead -
 * boards/LP_AM243/common/ holds the AM243x MSRAM map, the R5F<->M4F IPC
 * contract, the boot report and the stack-painting helper.
 *
 * Signable header (also the first 32 bytes stored on flash):
 *     HDR_FMT = "<8I"
 *     pack(MAGIC, HDR_VERSION, image_size, algo_id, 0,0,0,0)
 *
 * On-flash memory map (STM32L4R5; AM243 places the equivalents in OSPI):
 *     0x08000000  bootloader (secure-boot verifier)
 *     0x0801F000  public key (4 KiB page)            -> ends at 0x08020000
 *     0x08020000  signed_app.bin  { header region (8 KiB) + app image }
 *     0x08022000    app image (inside signed_app.bin, right after the header region)
 *
 * signed_app.bin is ONE CONTIGUOUS blob:
 *     [0]              signable header (32 B)
 *     [32]             signature       (ML-DSA; length by algo_id)
 *     [..PQSB_RESERVE) 0xFF padding
 *     [PQSB_RESERVE]   image           (image_size bytes)   (PQSB_RESERVE = 8 KiB)
 *   The app image is contiguous with the header region (no gap between them).
 *   The public key is a SEPARATE 4 KiB region, not part of signed_app.bin.
 *
 * Signed message (note: header IS covered, so metadata is tamper-protected):
 *     M   = SHA2-256( header[0:32]  ||  image[0:image_size] )
 *     sig = ML-DSA.sign(sk, M)          // pure ML-DSA, EMPTY ctx
 *
 * Device verify (single step):
 *     d  = SHA2-256( header || image )
 *     ok = ML-DSA.verify(pk, d, sig, ctx="")
 *
 * AM243 note: the digest covers header[0:32] || image, but the signature + 0xFF
 * padding sit between them in the (contiguous) blob, so the device hashes
 * TWO BYTE-RANGES - the 32-byte header, then the image from PQSB_RESERVE on -
 * and skips the middle. Hashing the whole blob would NOT match.
 */
#ifndef PQSB_MANIFEST_H
#define PQSB_MANIFEST_H

#include <stdint.h>

#define PQSB_MAGIC        0x53344D50u   /* 'PM4S' */
#define PQSB_HDR_VERSION  0x00000001u

/* algo_id encoding used by the Nucleo tool. */
#define PQSB_ALGO_ML_DSA_44  1u
#define PQSB_ALGO_ML_DSA_65  2u
#define PQSB_ALGO_ML_DSA_87  3u

#define PQSB_RESERVE   8192u   /* header region: 2 x 4 KiB pages */
#define PQSB_PAD_BYTE  0xFFu   /* header/pubkey padding fill      */

/* Pure ML-DSA, empty context (the Nucleo tool passes no ctx). */
#define PQSB_SIG_CTX      ""
#define PQSB_SIG_CTX_LEN  0u

#if defined(__GNUC__) || defined(__clang__)
#define PQSB_PACKED __attribute__((packed))
#else
#define PQSB_PACKED
#endif

typedef struct PQSB_PACKED {
    uint32_t magic;         /* PQSB_MAGIC        */
    uint32_t hdr_version;   /* PQSB_HDR_VERSION  */
    uint32_t image_size;    /* bytes of image covered by the hash */
    uint32_t algo_id;       /* PQSB_ALGO_ML_DSA_*                  */
    uint32_t reserved[4];   /* zero                               */
} pqsb_boot_hdr_t;

#define PQSB_HDR_LEN 32u

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(pqsb_boot_hdr_t) == PQSB_HDR_LEN,
               "pqsb_boot_hdr_t must be exactly 32 packed bytes ('<8I')");
#endif

#define PQSB_SIG_LEN_44  2420u
#define PQSB_SIG_LEN_65  3309u
#define PQSB_SIG_LEN_87  4627u
#define PQSB_SIG_MAX     PQSB_SIG_LEN_87

#define PQSB_PUBKEY_LEN_44 1312u
#define PQSB_PUBKEY_LEN_65 1952u
#define PQSB_PUBKEY_LEN_87 2592u

#endif /* PQSB_MANIFEST_H */
