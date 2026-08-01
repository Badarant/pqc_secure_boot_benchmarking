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

/* Bootloader startup: same minimal pattern as the app. */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern const uint32_t vector_table[];
int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

void Reset_Handler(void)
{

    /* Activate the FPU. */
    *(volatile uint32_t *)0xE000ED88 |= (0xF << 20);   /* CPACR: CP10/CP11 full access */
    __asm__ volatile("dsb");
    __asm__ volatile("isb");


    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    *(volatile uint32_t *)0xE000ED08 = (uint32_t)vector_table; /* SCB->VTOR = our table */
    main();
    for (;;) {}
}

void Default_Handler(void) { for (;;) {} }

__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)Default_Handler, /* NMI */
    (uint32_t)Default_Handler, /* HardFault */
    (uint32_t)Default_Handler, /* MemManage */
    (uint32_t)Default_Handler, /* BusFault */
    (uint32_t)Default_Handler, /* UsageFault */
    0, 0, 0, 0,
    (uint32_t)Default_Handler, /* SVCall */
    (uint32_t)Default_Handler, /* DebugMonitor */
    0,
    (uint32_t)Default_Handler, /* PendSV */
    (uint32_t)Default_Handler, /* SysTick */
};
