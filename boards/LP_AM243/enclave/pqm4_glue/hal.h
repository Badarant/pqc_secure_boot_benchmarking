/*
 * hal.h - minimal shim for the pqm4 HAL header.
 *
 * pqm4's vendored ML-DSA m4f sources (poly.c, polyvec.c) include "hal.h"
 * unconditionally. On a real pqm4 board target that pulls in UART and
 * cycle-count helpers; the verify path used by `boot` needs none of them (the
 * DBENCH timing macros are compiled out when DBENCH is undefined).
 *
 * This shim lives in OUR tree and satisfies the include without touching
 * third_party_sw/ (R1). If a future pqm4 source calls a hal_ symbol on the
 * verify path, add a real definition here rather than editing upstream.
 */
#ifndef PQSB_PQM4_HAL_SHIM_H
#define PQSB_PQM4_HAL_SHIM_H

#include <stddef.h>
#include <stdint.h>

/* Present only so a stray reference still links; unused on the verify path. */
static inline void hal_send_str(const char *s) { (void)s; }

#endif /* PQSB_PQM4_HAL_SHIM_H */
