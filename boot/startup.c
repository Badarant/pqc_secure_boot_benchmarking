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

/* stack painting: fill unused stack region with a known marker.
   Paint from just above the heap end up to near the current SP,
   leaving a safety gap so we don't clobber our own frame. */
#define STACK_MARKER 0xDEADBEEFu

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack, end;
extern const uint32_t vector_table[];
int  main(void);
void Reset_Handler(void);
void Default_Handler(void);


static void paint_stack(void)
{
    uint32_t *p;
    register uint32_t sp __asm__("sp");
    /* paint from heap end up to ~256 bytes below current SP (safety gap) */
    for (p = &end; p < (uint32_t *)(sp - 256); p++) {
        *p = STACK_MARKER;
    }
}

uint32_t stack_used_bytes(void)
{
    uint32_t *p;
    /* scan upward from heap end; first non-marker word = deepest stack use */
    for (p = &end; p < &_estack; p++) {
        if (*p != STACK_MARKER)
            break;   /* found the high-water mark */
    }
    /* bytes from here to top of stack = max stack used */
    return (uint32_t)((uint8_t *)&_estack - (uint8_t *)p);
}

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
    paint_stack();
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
