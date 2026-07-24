/**
 * @file tuya_device_board.c
 * @brief RK3506B (Alientek ATK-DLRK3506B) board initialization.
 *
 * Minimal Linux board bring-up, modelled on the Ubuntu simulation board.
 * Wires the TuyaOS comm/debug UART. set_uart() is provided by the TuyaOS
 * platform layer (libtuyaos) and resolves at link time.
 *
 * @copyright Copyright (c) tuya.inc 2022
 */

#include "tuya_device_board.h"

/* UART device nodes used by the TuyaOS framework on RK3506B.
 *   - comm  UART: production-test / protocol channel
 *   - debug UART: log output (NULL => reuse comm)
 * RK3506B kernel debug console is ttyFIQ0 (UART0 @ 0xff0a0000). Choose a
 * free ttySx for TuyaOS and update if your pinmux differs. */
#define RK3506B_TUYA_COMM_UART     "/dev/ttyS1"
#define RK3506B_TUYA_DEBUG_UART    NULL

/**
 * @brief RK3506B board initialization.
 * @return OPRT_OK on success. Others on error (see tuya_error_code.h).
 */
OPERATE_RET tuya_device_board_init(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;

    extern VOID_T set_uart(CHAR_T *comm, CHAR_T *debug);
    set_uart(RK3506B_TUYA_COMM_UART, RK3506B_TUYA_DEBUG_UART);

    return rt;
}
