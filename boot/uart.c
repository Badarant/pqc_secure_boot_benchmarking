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

#include "uart.h"

/* --- LPUART1 on PG7(TX)/PG8(RX) -> ST-LINK VCP -> /dev/ttyACM0 --- */
#define RCC_AHB2ENR_U (*(volatile uint32_t *)(0x40021000UL + 0x4C))
#define RCC_APB1ENR2  (*(volatile uint32_t *)(0x40021000UL + 0x5C))  /* LPUART1 on APB1ENR2 */
#define PWR_CR2       (*(volatile uint32_t *)(0x40007000UL + 0x04))
#define RCC_APB1ENR1U (*(volatile uint32_t *)(0x40021000UL + 0x58))
#define GPIOG_MODER   (*(volatile uint32_t *)(0x48001800UL + 0x00))
#define GPIOG_AFRL    (*(volatile uint32_t *)(0x48001800UL + 0x20))
#define LPUART1_BRR   (*(volatile uint32_t *)(0x40008000UL + 0x0C))
#define LPUART1_CR1   (*(volatile uint32_t *)(0x40008000UL + 0x00))
#define LPUART1_ISR   (*(volatile uint32_t *)(0x40008000UL + 0x1C))
#define LPUART1_TDR   (*(volatile uint32_t *)(0x40008000UL + 0x28))

void uart_init(void)
{
    /* 1. activate VDDIO2 */
    RCC_APB1ENR1U |= (1u << 28);      /* PWR clock (bit 28) */
    PWR_CR2       |= (1u << 9);        /* IOSV: activate power supply PG[15:2] */

    /* 2. Clocks */
    RCC_AHB2ENR_U |= (1u << 6);        /* GPIOG clock (bit 6) */
    RCC_APB1ENR2  |= (1u << 0);        /* LPUART1 clock (bit 0) */

    /* 3. PG7, PG8 -> Alternate Function (10) */
    GPIOG_MODER &= ~((3u << (7*2)) | (3u << (8*2)));
    GPIOG_MODER |=  ((2u << (7*2)) | (2u << (8*2)));
    /* AF8 = LPUART1, for PG7/PG8 (in AFRL: pins 0-7) */
    GPIOG_AFRL &= ~(0xFu << (7*4));
    GPIOG_AFRL |=  (8u   << (7*4));    /* PG7 -> AF8 */
    /* PG8 in AFRH (pinii 8-15) */
    *(volatile uint32_t *)(0x48001800UL + 0x24) &= ~(0xFu << ((8-8)*4));
    *(volatile uint32_t *)(0x48001800UL + 0x24) |=  (8u   << ((8-8)*4));  /* PG8 -> AF8 */

    /* 4. Baud LPUART: BRR = (256 * f_ck) / baud. For MSI 4 MHz, 9600 baud:
          (256 * 4000000) / 9600 = 106667 */
    LPUART1_BRR = (256u * 4000000u) / 9600u;
    LPUART1_CR1 = (1u << 3) | (1u << 0);   /* TE + UE */
}

void uart_putc(char c)
{
    while (!(LPUART1_ISR & (1u << 7))) { }   /* TXE */
    LPUART1_TDR = (uint8_t)c;
}
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }


/* prints an uint32 decimal value */
void uart_put_u32(uint32_t v)
{
    char buf[11]; 
    int i = 10; 
    
    buf[10] = 0;

    if (v == 0) 
    { 
        uart_putc('0'); 
        return; 
    }

    while (v && i) 
    { 
        buf[--i] = '0' + (v % 10); 
        v /= 10; 
    }

    uart_puts(&buf[i]);
}