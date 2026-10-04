/*
 * pqsb_randombytes_stub.c - link-time stub for pqm4's randombytes().
 *
 * pqm4's ml-dsa sign.c is one translation unit holding keypair/signature/verify.
 * Only crypto_sign_verify_ctx() runs in `boot`, and it never calls randombytes()
 * - but the symbol must still resolve at link time. This stub lives in OUR tree
 * (third_party_sw/ is untouched, R1).
 *
 * It deliberately fails: if key generation or signing were ever invoked on the
 * device it would return non-zero and produce no entropy, rather than silently
 * handing out predictable bytes.
 */
#include <stddef.h>
#include <stdint.h>

int randombytes(uint8_t *output, size_t n);

int randombytes(uint8_t *output, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        output[i] = 0;
    }
    return -1;   /* not available on the verifier */
}
