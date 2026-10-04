/*
 * main.c - AM243x m4fss0-0 / nortos / ti-arm-clang entry for `enclave`.
 * Based on the MCU+ SDK m4f nortos example entry (System/Board init, then the
 * app function opens drivers and runs).
 */
#include <stdio.h>
#include <stdlib.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "ti_board_config.h"

#include "enclave.h"

int main(void)
{
#if PQSB_STACK_PAINT
    /* Before anything else, so the unpaintable floor is just this frame. The
     * M4F is ARMv7-M: one stack (MSP) serves both thread and handler mode, so
     * the IPC callback's frames are included in the same measurement. */
    enclave_stackPaint();
#endif

    System_init();
    Board_init();

    Drivers_open();
    Board_driversOpen();

    /* Unbuffered stdout: the CCS/DSS console (CSS log) reads DebugP_log output
     * via the C-RTS CIO path. enclave_main() never returns, so a buffered
     * stream would strand output in the C-RTS buffer. */
    setvbuf(stdout, NULL, _IONBF, 0);

    enclave_main(NULL);   /* does not return */

    Board_driversClose();
    Drivers_close();
    Board_deinit();
    System_deinit();

    return 0;
}
