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

/* Bare-metal blink on NUCLEO-L4R5ZI. Runs on default MSI clock (~4 MHz). */
#include <stdint.h>

#define RCC_BASE     0x40021000UL
#define RCC_AHB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x4C))  /* GPIO clock enable */
#define GPIOC_BASE   0x48000800UL
#define GPIOC_MODER  (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIOC_BSRR   (*(volatile uint32_t *)(GPIOC_BASE + 0x18)) /* atomic set/reset */

/* LD1 green = PC7 on NUCLEO-L4R5ZI. VERIFY against board manual UM2179;
   to use another LED, change the port/pin here. */
#define LED_PIN 7

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

int main(void)
{
    RCC_AHB2ENR |= (1u << 2);                 /* enable GPIOC clock (bit 2) */
    GPIOC_MODER &= ~(3u << (LED_PIN * 2));     /* clear the 2 mode bits */
    GPIOC_MODER |=  (1u << (LED_PIN * 2));     /* 01 = general purpose output */

    for (;;) {
        GPIOC_BSRR = (1u << LED_PIN);          /* set bit  -> LED on  */
        delay(400000);
        GPIOC_BSRR = (1u << (LED_PIN + 16));   /* reset bit -> LED off */
        delay(400000);
    }
}
