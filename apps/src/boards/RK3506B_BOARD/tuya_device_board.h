/**
 * @file tuya_device_board.h
 * @brief RK3506B (Alientek ATK-DLRK3506B) board definition.
 *
 * Mirrors the Linux/Ubuntu simulation board: a minimal
 * tuya_device_board_init() that wires the TuyaOS comm/debug UART.
 * Extend with audio codec / keys / LED / display / network init as each
 * peripheral is validated on the RK3506B.
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_DEVICE_BOARD_H__
#define __TUYA_DEVICE_BOARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_board_config.h"

/**
 * @brief RK3506B board initialization.
 * @return OPRT_OK on success. Others on error (see tuya_error_code.h).
 */
OPERATE_RET tuya_device_board_init();

#ifdef __cplusplus
}
#endif
#endif /* __TUYA_DEVICE_BOARD_H__ */
