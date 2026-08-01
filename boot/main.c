/*
 * Copyright 2026 Liviu Silaghe liviu.silaghe@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Secured bootloader: signal, verify app signature, then hand off to the application at 0x08021000. */
#include <stdint.h>
#include <stddef.h>
#include "api.h" 
#include "fips202.h"
#include "uart.h"

#define APP_BASE     0x08021000UL

#define RCC_AHB2ENR  (*(volatile uint32_t *)(0x40021000UL + 0x4C))
#define GPIOB_MODER  (*(volatile uint32_t *)(0x48000400UL + 0x00))
#define GPIOB_BSRR   (*(volatile uint32_t *)(0x48000400UL + 0x18))
#define RED_PIN      14   /* LD3 red = PB14 (bootloader "alive" indicator) */
#define BLUE_PIN     7   /* LD2 blue = PB7 */

#define PUBKEY_ADDR   ((const uint8_t *)0x0801F000)   /* public key */
#define HEADER_ADDR   ((const uint8_t *)0x08020000)   /* signed header */
#define APP_ADDR      ((const uint8_t *)0x08021000)   /* app image */

#define HDR_MAGIC     0x53344D50u
#define HDR_SIGNABLE  32
#define SIG_OFFSET    32

/* --- DWT cycle counter (Cortex-M4) --- */
#define DEMCR       (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)

/* cycles counter for the app signature verification*/
volatile uint32_t verify_cycles = 0;

static void cycle_counter_init(void)
{
    DEMCR    |= (1u << 24);     /* TRCENA: trace/DWT unit activation*/
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;             /* CYCCNTENA: start the cycles monitor */
}


int PQCLEAN_randombytes(uint8_t *buf, size_t n) { (void)buf; (void)n; return -1; }

static void gpiob_output(int pin) {
    RCC_AHB2ENR |= (1u << 1);
    GPIOB_MODER &= ~(3u << (pin * 2));
    GPIOB_MODER |=  (1u << (pin * 2));
}

static void sha3_256_two(uint8_t out[32],
                         const uint8_t *a, size_t alen,
                         const uint8_t *b, size_t blen)
{
    sha3_256incctx ctx __attribute__((aligned(8)));
    sha3_256_inc_init(&ctx);
    sha3_256_inc_absorb(&ctx, a, alen);   /* add the signable header (32 B) */
    sha3_256_inc_absorb(&ctx, b, blen);   /* app image from flash */
    sha3_256_inc_finalize(out, &ctx);
}

/* header layout (little-endian): magic, hdr_version, image_size, algo_id, rez[4] */
static uint32_t hdr_u32(int i) { return ((const uint32_t *)HEADER_ADDR)[i]; }

static int verify_app(void)
{
    if (hdr_u32(0) != HDR_MAGIC) 
        return -1;            /* invalid magic */
    uint32_t image_size = hdr_u32(2);
    uint32_t algo_id    = hdr_u32(3);
    
    if (algo_id != 1) 
        return -2;                       /* unsupported algorithm */


    uint8_t digest[32];

    sha3_256_two(digest, HEADER_ADDR, HDR_SIGNABLE, APP_ADDR, image_size);

    const uint8_t *sig = HEADER_ADDR + SIG_OFFSET;
    return PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify(
        sig, PQCLEAN_MLDSA65_CLEAN_CRYPTO_BYTES,
        digest, sizeof digest,
        PUBKEY_ADDR);                                  /* 0 = valid */
}

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

/* Optional: brief red pulse so you can SEE the bootloader run before handoff. */
static void signal_bootloader(void)
{
    RCC_AHB2ENR |= (1u << 1);                   /* GPIOB clock */
    GPIOB_MODER &= ~(3u << (RED_PIN * 2));
    GPIOB_MODER |=  (1u << (RED_PIN * 2));       /* PB14 output */
    GPIOB_BSRR = (1u << RED_PIN);                /* red on  */
    delay(800000);
    GPIOB_BSRR = (1u << (RED_PIN + 16));         /* red off */
}

static void jump_to_app(void)
{
    uint32_t app_msp   = *(volatile uint32_t *)(APP_BASE + 0);
    uint32_t app_reset = *(volatile uint32_t *)(APP_BASE + 4);

    /* point the vector table at the application before jumping */
    *(volatile uint32_t *)0xE000ED08 = APP_BASE; /* SCB->VTOR */

    __asm__ volatile("cpsid i");                 /* disable IRQs during the switch */
    /* set MSP and branch to the app reset handler in one atomic step:
       both values are already in registers, so the stack switch can't corrupt them */
    __asm__ volatile(
        "msr msp, %0\n"
        "bx  %1\n"
        :: "r"(app_msp), "r"(app_reset) : "memory");
    /* never returns */
}

int main(void)
{
    cycle_counter_init();
    uart_init();

    signal_bootloader();
        
    uint32_t t0 = DWT_CYCCNT;
    int r = verify_app(); 
    uint32_t cycles = DWT_CYCCNT - t0;

    uart_puts("verify cycles: ");
    uart_put_u32(cycles);
    uart_puts("\r\n");

    if (r == 0) 
    { 
        uart_puts("VALID -> boot\r\n"); /* blue LED + jump */ 
        gpiob_output(BLUE_PIN);
        GPIOB_BSRR = (1u << BLUE_PIN);   /* permament blue > verrification passed */
        jump_to_app();                    /* blinking green -> app running */
    }

    uart_puts("INVALID -> halt\r\n"); /* rosu */ 
    gpiob_output(RED_PIN);
    GPIOB_BSRR = (1u << RED_PIN);         /* permanent red -> verification failed */

    for (;;) {}
}