/*
 * stack_paint.h - stack high-water-mark measurement by painting.
 *
 * Copyright 2026 Liviu Silaghe, licensed under Apache-2.0 (see LICENSE).
 *
 * The linker map tells us how much stack is RESERVED, which for this project
 * says nothing useful: `boot`'s .stack is sized for config B's PQClean verify
 * and is wildly over-provisioned for config A. What the
 * benchmark needs is how much is actually USED, and that cannot be derived
 * offline - the SDK's prebuilt libraries (OSPI/UDMA/Sciclient/printf) ship no
 * -fstack-usage data, so a static call-graph bound stops at the first library
 * call. Painting measures the real peak, library frames included.
 *
 * Method: fill the unused part of the stack with a known pattern before the
 * work starts, then scan from the far end afterwards for the first word that
 * still holds it. Everything past that point was written by some frame.
 *
 * Two limits, both benign here and both worth stating when reporting:
 *  - the region above the painting call's own frame cannot be painted (we are
 *    standing on it), so a peak shallower than that frame reads as exactly
 *    that frame. Paint as early as possible and the floor stays tiny;
 *  - a frame that legitimately stores PQSB_STACK_PATTERN at the deepest word
 *    reached would hide itself. The pattern is odd and non-zero to make that
 *    as unlikely as it can be made.
 *
 * Both cores' linkers export __STACK_END (the HIGH address - these stacks grow
 * down) and __STACK_SIZE, so the caller needs no hardcoded addresses.
 */
#ifndef PQSB_STACK_PAINT_H
#define PQSB_STACK_PAINT_H

#include <stdint.h>

#define PQSB_STACK_PATTERN  0xC0DEDEADu

/*
 * Fill [base, limit) with the pattern. `limit` is normally the current stack
 * pointer minus a small margin, so the caller's own frame survives.
 */
static inline void pqsb_stack_paint(uint32_t *base, const uint32_t *limit)
{
    while (base < limit) {
        *base++ = PQSB_STACK_PATTERN;
    }
}

/*
 * Peak usage in bytes: scan up from `base` for the first word that is no
 * longer the pattern, and report the distance from there to `end` (the high
 * address the stack grows down from).
 */
static inline uint32_t pqsb_stack_peak(const uint32_t *base, const uint32_t *end)
{
    const uint32_t *p = base;

    while ((p < end) && (*p == PQSB_STACK_PATTERN)) {
        p++;
    }
    return (uint32_t)((uintptr_t)end - (uintptr_t)p);
}

#endif /* PQSB_STACK_PAINT_H */
