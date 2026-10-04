/* SPDX-License-Identifier: Apache-2.0
 *
 * main.c - AM243x-LP test application payload for the PQC secure-boot demo.
 *
 * Prints a banner on MAIN UART0 (the LaunchPad's XDS110 USB virtual COM port,
 * 115200 8N1) 
 *
 *     [app] App loaded by SBL, started and running
 *     [app] done
 *
 * Bare-metal: pokes the 16550-style UART directly. UART0 is already powered,
 * clocked and pinmuxed by the SBL (SBL NULL / sbl_ospi) that ran before us; the
 * light re-init below just re-asserts 115200 8N1 so the app also works if run
 * standalone.
 */
#include <stdint.h>

#define UART0_BASE   0x02800000u
#define UART_REG(o)  (*(volatile uint32_t *)(UART0_BASE + (o)))

#define UART_THR     0x00u   /* tx holding (DLL when DLAB=1)   */
#define UART_DLH     0x04u   /* divisor high (DLAB=1)          */
#define UART_FCR     0x08u   /* fifo control                   */
#define UART_LCR     0x0Cu   /* line control                   */
#define UART_LSR     0x14u   /* line status                    */
#define UART_MDR1    0x20u   /* mode definition 1              */

#define UART_LSR_TX_EMPTY  (1u << 5)   /* THR empty */

/* MAIN UART0 functional clock is 48 MHz; 48e6 / (16 * 115200) ~= 26 */
#define UART_DIV_115200   26u

static void uart_init(void)
{
    UART_REG(UART_MDR1) = 0x7u;              /* disable while configuring   */
    UART_REG(UART_LCR)  = 0x83u;             /* DLAB=1, 8 data, 1 stop, no parity */
    UART_REG(UART_THR)  = UART_DIV_115200;   /* DLL */
    UART_REG(UART_DLH)  = 0x00u;             /* DLH */
    UART_REG(UART_LCR)  = 0x03u;             /* DLAB=0, 8N1                  */
    UART_REG(UART_FCR)  = 0x07u;             /* enable + clear rx/tx FIFOs   */
    UART_REG(UART_MDR1) = 0x0u;              /* UART 16x mode                */
}

static void uart_putc(char c)
{
    while ((UART_REG(UART_LSR) & UART_LSR_TX_EMPTY) == 0u) {
        /* wait */
    }
    UART_REG(UART_THR) = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

static void uart_puthex_bytes(const uint8_t *p, uint32_t n)
{
    static const char hex[] = "0123456789abcdef";
    for (uint32_t i = 0u; i < n; i++) {
        uart_putc(hex[p[i] >> 4]);
        uart_putc(hex[p[i] & 0xFu]);
    }
}


int main(void)
{

    uart_init();
    uart_puts("\r\n[app] App loaded by SBL, started and running\r\n");

    uart_puts("[app] done\r\n");
    for (;;) {
        /* stop here - nothing further to report */
    }
}
