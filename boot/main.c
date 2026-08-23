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
#include "clock.h"
#if defined(HASH_SHA_256)
#include "sha2.h"
#endif

#if defined(IMPL_CLEAN) && defined(SCHEME_44)
  #define MLDSA_VERIFY    PQCLEAN_MLDSA44_CLEAN_crypto_sign_verify
  #define MLDSA_SIG_BYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES
  #define MLDSA_PK_BYTES  PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES
  #define IMPL_SCHEME     "CLEAN-ML-DSA-44"
#elif defined(IMPL_CLEAN) && defined(SCHEME_65)
  #define MLDSA_VERIFY    PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify
  #define MLDSA_SIG_BYTES PQCLEAN_MLDSA65_CLEAN_CRYPTO_BYTES
  #define MLDSA_PK_BYTES  PQCLEAN_MLDSA65_CLEAN_CRYPTO_PUBLICKEYBYTES
  #define IMPL_SCHEME     "CLEAN-ML-DSA-65"
#elif defined(IMPL_CLEAN) && defined(SCHEME_87)
  #define MLDSA_VERIFY    PQCLEAN_MLDSA87_CLEAN_crypto_sign_verify
  #define MLDSA_SIG_BYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_BYTES
  #define MLDSA_PK_BYTES  PQCLEAN_MLDSA87_CLEAN_CRYPTO_PUBLICKEYBYTES
  #define IMPL_SCHEME     "CLEAN-ML-DSA-87"
#elif defined(IMPL_M4F)
  #define MLDSA_VERIFY    crypto_sign_verify           /* macro din api.h m4f */
  #define MLDSA_SIG_BYTES CRYPTO_BYTES
  #define MLDSA_PK_BYTES  CRYPTO_PUBLICKEYBYTES
  #ifdef SCHEME_44
    #define IMPL_SCHEME     "M4F-ML-DSA-44"
  #elif SCHEME_65
    #define IMPL_SCHEME     "M4F-ML-DSA-65"
  #elif SCHEME_87
    #define IMPL_SCHEME     "M4F-ML-DSA-87"
  #endif
#endif

#define APP_BASE     0x08022000UL

#define RCC_AHB2ENR  (*(volatile uint32_t *)(0x40021000UL + 0x4C))
#define GPIOB_MODER  (*(volatile uint32_t *)(0x48000400UL + 0x00))
#define GPIOB_BSRR   (*(volatile uint32_t *)(0x48000400UL + 0x18))
#define RED_PIN      14   /* LD3 red = PB14 (bootloader "alive" indicator) */
#define BLUE_PIN     7   /* LD2 blue = PB7 */

#define PUBKEY_ADDR   ((const uint8_t *)0x0801F000)   /* public key */
#define HEADER_ADDR   ((const uint8_t *)0x08020000)   /* signed header */
#define APP_ADDR      ((const uint8_t *)0x08022000)   /* app image */

#define HDR_MAGIC     0x53344D50u
#define HDR_SIGNABLE  32
#define SIG_OFFSET    32

#if defined(SCHEME_44)
  #define EXPECTED_ALGO_ID 1
#elif defined(SCHEME_65)
  #define EXPECTED_ALGO_ID 2
#elif defined(SCHEME_87)
  #define EXPECTED_ALGO_ID 3
#endif

/* --- DWT cycle counter (Cortex-M4) --- */
#define DEMCR       (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)

extern void clock_init(void);
extern const uint32_t g_sysclk_hz;
extern uint32_t stack_used_bytes(void);

/* cycles counter for the app signature verification*/
volatile uint32_t verify_cycles = 0;
volatile uint32_t cyc_sha3   = 0;
volatile uint32_t cyc_verify = 0;

/* digest computed over header+image, kept for UART dump in main() */
static uint8_t g_digest[32];

static void cycle_counter_init(void)
{
    DEMCR    |= (1u << 24);     /* TRCENA: trace/DWT unit activation*/
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;             /* CYCCNTENA: start the cycles monitor */
}


// #ifdef IMPL_CLEAN
// int PQCLEAN_randombytes(uint8_t *buf, size_t n) { (void)buf; (void)n; return -1; }
// #endif
// #ifdef IMPL_M4F
// int randombytes(uint8_t *buf, size_t n) { (void)buf; (void)n; return -1; }
// #endif
int PQCLEAN_randombytes(uint8_t *buf, size_t n) { (void)buf; (void)n; return -1; }

static void gpiob_output(int pin) {
    RCC_AHB2ENR |= (1u << 1);
    GPIOB_MODER &= ~(3u << (pin * 2));
    GPIOB_MODER |=  (1u << (pin * 2));
}


#if defined(HASH_SHA3_256)
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
#elif  defined(HASH_SHA_256)
static void sha_256_two(uint8_t out[32],
                         const uint8_t *a, size_t alen,
                         const uint8_t *b, size_t blen)
{
    sha256ctx ctx __attribute__((aligned(8)));
    uint8_t first[64];
    size_t i, take;

    sha256_inc_init(&ctx);

    /* Unlike SHA3's inc_absorb(), sha256_inc_blocks() counts 64-byte BLOCKS and
       requires block-aligned input, so the 32-byte header cannot be absorbed on
       its own: it has to be merged with the head of the image into block 0. */
    if (alen + blen <= 64) {
        for (i = 0; i < alen; i++) first[i] = a[i];
        for (i = 0; i < blen; i++) first[alen + i] = b[i];
        sha256_inc_finalize(out, &ctx, first, alen + blen);
        return;
    }

    take = 64 - alen;                          /* bytes of b that fill block 0 */
    for (i = 0; i < alen; i++) first[i] = a[i];
    for (i = 0; i < take; i++) first[alen + i] = b[i];
    sha256_inc_blocks(&ctx, first, 1);

    /* inc_finalize() hashes every full block of its input, then pads the tail */
    sha256_inc_finalize(out, &ctx, b + take, blen - take);
}
#endif


/* header layout (little-endian): magic, hdr_version, image_size, algo_id, rez[4] */
static uint32_t hdr_u32(int i) { return ((const uint32_t *)HEADER_ADDR)[i]; }

static int verify_app(void)
{
    if (hdr_u32(0) != HDR_MAGIC) return -1;
    uint32_t image_size = hdr_u32(2);
    uint32_t algo_id    = hdr_u32(3);
    if (algo_id != EXPECTED_ALGO_ID) return -2;

    /* --- phase 1: digest SHA3-256 over the signed header+image --- */
    uint8_t *digest = g_digest;
    uint32_t t0 = DWT_CYCCNT;
    #if defined(HASH_SHA3_256)
    sha3_256_two(digest, HEADER_ADDR, HDR_SIGNABLE, APP_ADDR, image_size);
    #elif defined(HASH_SHA_256)
    sha_256_two(digest, HEADER_ADDR, HDR_SIGNABLE, APP_ADDR, image_size);
    #endif

    cyc_sha3 = DWT_CYCCNT - t0;

    /* --- phase 2: ML-DSA signature verification over digest --- */
    const uint8_t *sig = HEADER_ADDR + SIG_OFFSET;
    uint32_t t1 = DWT_CYCCNT;
    int r = MLDSA_VERIFY(
        sig, MLDSA_SIG_BYTES,
        digest, sizeof g_digest, PUBKEY_ADDR);
    cyc_verify = DWT_CYCCNT - t1;

    return r;
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
    clock_init();
    cycle_counter_init();
    uart_init();

    signal_bootloader();
        
    int r = verify_app(); 
    
    uart_puts(IMPL_SCHEME); uart_puts(" frequency [MHz]: "); uart_put_u32(SYSCLK_HZ/1000000);      uart_puts("\r\n");   
    uart_puts("image_size: "); uart_put_u32(hdr_u32(2));      uart_puts("\r\n");
#if defined(HASH_SHA3_256)
    uart_puts("sha3-256 cycles:   "); uart_put_u32(cyc_sha3);        uart_puts("\r\n");
#elif defined(HASH_SHA_256)
    uart_puts("sha-256 cycles:   "); uart_put_u32(cyc_sha3);        uart_puts("\r\n");
#endif
    uart_puts("verify cycles: "); uart_put_u32(cyc_verify);      uart_puts("\r\n");
    uart_puts("total cycles:  "); uart_put_u32(cyc_sha3+cyc_verify); uart_puts("\r\n");
    uart_puts("stack usage:  "); uart_put_u32(stack_used_bytes()); uart_puts("bytes\r\n");

    if (r == 0) 
    { 
        uart_puts("VALID -> boot\r\n"); /* blue LED + jump */ 
        gpiob_output(BLUE_PIN);
        GPIOB_BSRR = (1u << BLUE_PIN);   /* permament blue > verrification passed */
        jump_to_app();                    /* blinking green -> app running */
    }

    uart_puts("INVALID -> halt\r\n"); /* red */ 
    gpiob_output(RED_PIN);
    GPIOB_BSRR = (1u << RED_PIN);         /* permanent red -> verification failed */

    for (;;) {}
}