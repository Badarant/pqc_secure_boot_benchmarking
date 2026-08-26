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

#include <stddef.h>
#include <stdint.h>


#define POOL_SIZE 8192
static uint8_t alloc_pool[POOL_SIZE] __attribute__((aligned(8)));
static volatile size_t   alloc_off = 0;

__attribute__((used)) void *malloc(size_t n)
{
    /* allign offset to 8bytes */
    alloc_off = (alloc_off + 7u) & ~7u;
    if (alloc_off + n > POOL_SIZE) return NULL;
    void *p = &alloc_pool[alloc_off];
    alloc_off += n;
    return p;
}

void free(void *p)
{
    (void)p;
}

/* pool reset */
void alloc_reset(void) 
{ 
    alloc_off = 0; 
}