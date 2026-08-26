#ifndef _CLOCK_H_
#define _CLOCK_H_
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

/* --- select PLL dividers + wait states + boot from target frequency --- */
#if defined(CPU_FREQ_20)
  #define PLL_M 2
  #define PLL_N 10
  #define PLL_R_DIV 4          /* PLLR = 4 -> field value below */
  #define FLASH_WS 0
  #define SYSCLK_HZ 20000000u
  #define FREQ_TEXT "@20MHz"
#elif defined(CPU_FREQ_40)
  #define PLL_M 2
  #define PLL_N 10
  #define PLL_R_DIV 2
  #define FLASH_WS 1
  #define SYSCLK_HZ 40000000u
    #define FREQ_TEXT "@40MHz"
#elif defined(CPU_FREQ_80)
  #define PLL_M 2
  #define PLL_N 20
  #define PLL_R_DIV 2
  #define FLASH_WS 3
  #define SYSCLK_HZ 80000000u
    #define FREQ_TEXT "@80MHz"
#elif defined(CPU_FREQ_120)
  #define PLL_M 2
  #define PLL_N 30
  #define PLL_R_DIV 2
  #define FLASH_WS 5
  #define NEED_BOOST
  #define SYSCLK_HZ 120000000u
  #define FREQ_TEXT "@120MHz"
#else
  /* no PLL frequency selected -> stay on default MSI ~4 MHz */
  #define SYSCLK_HZ 4000000u
  #define FREQ_TEXT "@4MHz"
#endif

/* PLLR field encoding in RCC_PLLCFGR bits[26:25]: 00=/2, 01=/4, 10=/6, 11=/8 */
#if PLL_R_DIV == 2
  #define PLL_R_FIELD 0u
#elif PLL_R_DIV == 4
  #define PLL_R_FIELD 1u
#endif

#endif