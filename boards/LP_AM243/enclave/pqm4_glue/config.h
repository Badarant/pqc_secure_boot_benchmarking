/*
 * config.h - build-time scheme selector for the vendored pqm4 ML-DSA sources.
 *
 * This file lives in OUR tree and is force-included (-include) ahead of every
 * pqm4 translation unit, so it defines CONFIG_H and DILITHIUM_MODE before
 * pqm4's own per-scheme config.h is reached (that file is guarded by
 * "#ifndef CONFIG_H" and therefore no-ops). third_party_sw/ is never edited
 * (R1); this is configuration-only glue.
 *
 * Select the parameter set with -DPQSB_SCHEME=44|65|87 (default 65).
 *   ML-DSA-44 -> DILITHIUM_MODE 2
 *   ML-DSA-65 -> DILITHIUM_MODE 3
 *   ML-DSA-87 -> DILITHIUM_MODE 5
 */
#ifndef CONFIG_H
#define CONFIG_H

#ifndef PQSB_SCHEME
#define PQSB_SCHEME 65
#endif

#if   (PQSB_SCHEME == 44)
#define DILITHIUM_MODE 2
#elif (PQSB_SCHEME == 65)
#define DILITHIUM_MODE 3
#elif (PQSB_SCHEME == 87)
#define DILITHIUM_MODE 5
#else
#error "PQSB_SCHEME must be 44, 65 or 87"
#endif

/* Keep the reference small-signing strategy default (unused on the verify path). */
/* #define SIGN_STACKSTRATEGY 2 */

#endif /* CONFIG_H */
