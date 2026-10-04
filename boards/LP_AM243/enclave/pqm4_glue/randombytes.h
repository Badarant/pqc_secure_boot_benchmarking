/*
 * randombytes.h - prototype shim for the pqm4 ML-DSA sources.
 *
 * The vendored sign.c includes "randombytes.h". Upstream that resolves (on a
 * POSIX checkout) to mupq/pqclean/common/randombytes.h via a symlink that does
 * not survive a Windows checkout. This shim in OUR tree provides the same
 * prototype so the file compiles; the symbol is satisfied at link time by
 * pqm4_glue/pqsb_randombytes_stub.c (third_party_sw/ untouched, R1).
 *
 * Only crypto_sign_verify_ctx() runs in `boot`, and it never calls randombytes().
 */
#ifndef PQSB_PQM4_RANDOMBYTES_SHIM_H
#define PQSB_PQM4_RANDOMBYTES_SHIM_H

#include <stddef.h>
#include <stdint.h>

int randombytes(uint8_t *output, size_t n);

#endif /* PQSB_PQM4_RANDOMBYTES_SHIM_H */
