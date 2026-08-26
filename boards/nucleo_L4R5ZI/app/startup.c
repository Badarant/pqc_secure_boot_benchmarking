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

/* Minimal startup: vector table + Reset_Handler. No HAL, no libc. */
#include <stdint.h>
extern const uint32_t vector_table[];
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

void Reset_Handler(void)
{
    /* copy .data initial values from FLASH to RAM */
    uint32_t *src = &_sidata, *dst = &_sdata;

    while (dst < &_edata) 
    {
        *dst++ = *src++;
    }

    /* zero .bss */
    for (dst = &_sbss; dst < &_ebss; ) 
    {
        *dst++ = 0;
    }

    /* point the vector table at THIS image, so the app also works standalone */

    *(volatile uint32_t*)0xE000ED08 = (uint32_t)vector_table; /* SCB->VTOR */
    main();
    for (;;) {}
}

void Default_Handler(void) { for (;;) {} }

/* Cortex-M core vector table (first 16 entries). No peripheral IRQs needed to blink. */
__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,        /* 0  initial MSP  <- bootloader reads this */
    (uint32_t)Reset_Handler,   /* 1  reset        <- bootloader jumps here */
    (uint32_t)Default_Handler, /* 2  NMI          */
    (uint32_t)Default_Handler, /* 3  HardFault    */
    (uint32_t)Default_Handler, /* 4  MemManage    */
    (uint32_t)Default_Handler, /* 5  BusFault     */
    (uint32_t)Default_Handler, /* 6  UsageFault   */
    0, 0, 0, 0,                /* 7-10 reserved   */
    (uint32_t)Default_Handler, /* 11 SVCall       */
    (uint32_t)Default_Handler, /* 12 DebugMonitor */
    0,                         /* 13 reserved     */
    (uint32_t)Default_Handler, /* 14 PendSV       */
    (uint32_t)Default_Handler, /* 15 SysTick      */
};
