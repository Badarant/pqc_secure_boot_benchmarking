/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * Satisfies the randombytes link symbol for config B. `boot` only ever calls
 * ML-DSA *verify*, which needs no randomness; this exists so PQClean's sign.c
 * links, and fails loudly if the signing path is ever reached by mistake.
 */
#include <stdint.h>
#include <stddef.h>
#include <kernel/dpl/DebugP.h>
#include "randombytes.h"

int randombytes(uint8_t *output, size_t n)
{
    (void)output;
    (void)n;
    DebugP_log("boot: randombytes() called - boot must never sign\r\n");
    for (;;) {
    }
}
