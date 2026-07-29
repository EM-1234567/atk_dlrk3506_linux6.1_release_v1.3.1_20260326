/**
 * @file tuya_adv.h
 * @brief BlueZ LEAdvertisingManager1 advertising for Tuya BLE netcfg
 * @version 1.0
 * @date 2026-07-24
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __TUYA_ADV_H__
#define __TUYA_ADV_H__

#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LE advertisement D-Bus object on shared connection
 * @param[in] conn system bus connection (from tuya_gatt_init)
 * @return LE_SUCCESS on success, error code on failure
 * @note Requires g_dbus_attach_object_manager() already done on conn.
 */
int tuya_adv_init(DBusConnection *conn);

/**
 * @brief Cache HCI-slot advertising parameters (interval in 0.625ms units)
 * @param[in] min_interval minimum advertising interval
 * @param[in] max_interval maximum advertising interval
 * @param[in] advtype unused (BlueZ Type is peripheral)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_params(uint16_t min_interval, uint16_t max_interval, uint8_t advtype);

/**
 * @brief Cache and parse advertising payload (HCI AD format)
 * @param[in] data advertising payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_data(uint8_t *data, uint8_t len);

/**
 * @brief Cache and parse scan response payload (HCI AD format)
 * @param[in] data scan response payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_scan_rsp(uint8_t *data, uint8_t len);

/**
 * @brief Enable or disable advertising via RegisterAdvertisement
 * @param[in] enable true to enable, false to disable
 * @return LE_SUCCESS if request accepted (may complete asynchronously)
 */
int tuya_adv_set_enable(bool enable);

/**
 * @brief Whether LEAdvertisingManager1 proxy is ready
 * @return true if manager proxy is available
 */
bool tuya_adv_manager_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_ADV_H__ */
