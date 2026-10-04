/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 Liviu Silaghe
 *
 * randombytes.h - declaration-only shim that shadows PQClean's own header for
 * config B (see boot/makefile).
 *
 * PQClean's common/randombytes.h pulls in <unistd.h>, which does not exist in
 * this bare-metal TI ARM Clang build. Only sign.c's *signing* path needs random
 * bytes; `boot` never signs, it only verifies, so nothing here is ever called -
 * the symbol exists purely so the unused code path links. Mirrors the same
 * trick the enclave uses for pqm4 (pqm4_glue/randombytes.h).
 *
 * R1: third_party_sw/ is never edited - this shadows the header from outside.
 */
#ifndef PQCLEAN_RANDOMBYTES_H
#define PQCLEAN_RANDOMBYTES_H

#include <stdint.h>
#include <stddef.h>

#define randombytes PQCLEAN_randombytes
int randombytes(uint8_t *output, size_t n);

#endif /* PQCLEAN_RANDOMBYTES_H */
