/*
 * Copyright 2026 Liviu Silaghe liviu.silaghe@gmail.com
 * SPDX-License-Identifier: Apache-2.0
 *
 * System clock configuration for STM32L4R5 (NUCLEO-L4R5ZI).
 * Target frequency is selected at compile time via -DCPU_FREQ_xx.
 *
 * Clock source: HSI16 (16 MHz internal RC) -> PLL -> SYSCLK.
 * PLL: SYSCLK = (HSI16 / M) * N / R, with VCO = (HSI16/M)*N in [64, 344] MHz.
 *
 * Flash wait states (voltage range 1, from ST HAL L4Rx/L4Sx table):
 *   0..20 MHz = 0 WS | 20..40 = 1 WS | 40..60 = 2 WS
 *   60..80 MHz = 3 WS | 80..100 = 4 WS | 100..120 = 5 WS
 * 120 MHz additionally requires voltage range 1 BOOST.
 */
#include <stdint.h>
#include "clock.h"

#define FLASH_ACR    (*(volatile uint32_t *)0x40022000)
#define RCC_CR       (*(volatile uint32_t *)0x40021000)
#define RCC_CFGR     (*(volatile uint32_t *)0x40021008)
#define RCC_PLLCFGR  (*(volatile uint32_t *)0x4002100C)
#define RCC_APB1ENR1 (*(volatile uint32_t *)0x40021058)
#define PWR_CR1      (*(volatile uint32_t *)0x40007000)
#define PWR_CR5      (*(volatile uint32_t *)0x40007080)
#define PWR_SR2      (*(volatile uint32_t *)0x40007014)

/* Expose the configured frequency so UART baud and time calc stay in sync. */
const uint32_t g_sysclk_hz = SYSCLK_HZ;

void clock_init(void)
{
#if defined(CPU_FREQ_20) || defined(CPU_FREQ_40) || defined(CPU_FREQ_80) || defined(CPU_FREQ_120)

    /* 1. Turn on HSI16 and wait until stable (PLL input). */
    RCC_CR |= (1u << 8);
    while (!(RCC_CR & (1u << 10))) { }

    /* 2. Ensure PLL is OFF before touching PLLCFGR - writes to PLLCFGR are
       IGNORED while PLLON=1. Switch SYSCLK to MSI first, then stop the PLL. */
    RCC_CFGR &= ~0x3u;                           /* SW = 00 -> MSI */
    while (((RCC_CFGR >> 2) & 0x3u) != 0x0u) { } /* SWS = 00 confirmed */
    RCC_CR &= ~(1u << 24);                       /* PLLON = 0 */
    while (RCC_CR & (1u << 25)) { }              /* wait PLLRDY = 0 */

#ifdef NEED_BOOST
    /* 3. Enable PWR clock + voltage range 1 BOOST (needed for 120 MHz). */
    RCC_APB1ENR1 |= (1u << 28);
    PWR_CR5 &= ~(1u << 8);                        /* R1MODE=0 -> boost */
#endif

    /* 4. Flash wait states BEFORE raising frequency, + prefetch/cache. */
    FLASH_ACR = (FLASH_ACR & ~0xFu) | (FLASH_WS & 0xFu)
              | (1u << 8) | (1u << 9) | (1u << 10);
    while ((FLASH_ACR & 0xFu) != (FLASH_WS & 0xFu)) { }

    /* 5. Configure PLL (now allowed, PLL is stopped). */
    RCC_PLLCFGR = (2u << 0)
                | (((PLL_M - 1u) & 0x7u) << 4)
                | ((PLL_N & 0x7Fu) << 8)
                | (PLL_R_FIELD << 25)
                | (1u << 24);                    /* PLLREN */

    /* 6. Start PLL, wait for lock. */
    RCC_CR |= (1u << 24);                         /* PLLON */
    while (!(RCC_CR & (1u << 25))) { }            /* PLLRDY */

    /* 7. Switch SYSCLK to PLL. */
    RCC_CFGR = (RCC_CFGR & ~0x3u) | 0x3u;
    while (((RCC_CFGR >> 2) & 0x3u) != 0x3u) { }
#endif
}