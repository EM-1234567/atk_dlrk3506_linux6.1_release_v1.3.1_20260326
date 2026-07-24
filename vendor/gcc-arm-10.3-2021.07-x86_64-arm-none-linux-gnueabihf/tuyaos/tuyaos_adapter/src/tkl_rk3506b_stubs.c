/**
 * @file tkl_rk3506b_stubs.c
 * @brief Link-resolution stubs for ATK-DLRK3506B bring-up.
 *
 * Covers framework/app symbols that have no implementation on this platform
 * yet: the set_uart() board hook, the app-level audio_dump_write(), and a
 * few BLE protocol-layer entry points still unused on this bring-up path.
 *
 * Wi-Fi ops table symbol TKL_WIFI is provided by tkl_wifi.c (not here).
 *
 * C does not mangle symbol names, so simplified parameter types are
 * ABI-compatible with the original declarations.
 *
 * @copyright Copyright (c) 2024 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include <stdint.h>

/* Board hook: select the framework's comm/debug UART. No-op here (framework
 * uses its default; the board's own UART setup handles routing). */
VOID_T set_uart(CHAR_T *comm, CHAR_T *debug) { (void)comm; (void)debug; }

/* App-level audio dump (declared in src/miscs/audio_analysis/audio_dump.h),
 * not implemented on this platform — no-op. */
void audio_dump_write(int type, uint8_t *data, uint16_t datalen) { (void)type; (void)data; (void)datalen; }

/* BLE protocol-layer stubs (from svc_bt / svc_upgrade). BLE SDK not present;
 * device activates with embedded UUID/AUTHKEY, so these stay no-ops. */
OPERATE_RET tuya_ble_reg_wifi_protect_cmd(void *cb)              { (void)cb; return OPRT_NOT_SUPPORTED; }
OPERATE_RET tuya_ble_resp_wifi_protect_to_app(void *data, unsigned int len) { (void)data; (void)len; return OPRT_NOT_SUPPORTED; }
void        tuya_ble_upgrade_deinit(void)                         { }
