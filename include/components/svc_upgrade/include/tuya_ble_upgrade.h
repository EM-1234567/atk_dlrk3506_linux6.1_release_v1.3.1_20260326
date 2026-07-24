/*
tuya_ble_upgrade.h
*/
#ifndef _TUYA_BLE_UPGRADE_H
#define _TUYA_BLE_UPGRADE_H

#include "tuya_cloud_types.h"
#include "tal_sw_timer.h"
#include "tal_hash.h"
#include "tal_mutex.h"
#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct {
    FW_UG_S fw;
    BYTE_T *buf;
    UINT_T buf_len; /* len为buffer容量 */
    UINT_T buf_off;/*当前ota数据偏移*/
    UINT_T file_offset;/*ota文件数据偏移*/
    UINT_T total_size; /* 文件总长度 */
    //md5 handle
    TKL_HASH_HANDLE  file_md5_ctx;
    MUTEX_HANDLE mutex; // mutex for thread safety

    TIMER_ID ble_ota_timer;
    PVOID_T p_upg_mgr;
} ble_upgrade_info_s, *p_ble_upgrade_info_s;

/**
 * @brief Initiates the BLE firmware upgrade process.
 *
 * This function starts the BLE upgrade on the specified OTA channel by validating 
 * and preparing the firmware image for upgrade. It verifies the integrity of the file 
 * using the provided MD5 hash.
 *
 * @param ota_channel The channel identifier used for the OTA upgrade.
 * @param ota_version The version of the firmware to which the device will be upgraded.
 * @param file_len The length of the firmware file in bytes.
 * @param md5 Pointer to the MD5 hash for the firmware file. Used for integrity verification.
 *
 * @return OPERATE_RET The operation status indicating success or an error code.
 */
OPERATE_RET tuya_ble_upgrade_start(CONST UINT32_T ota_channel, CONST UINT32_T ota_version, CONST UINT32_T file_len, CONST UINT8_T *md5);

/**
 * @brief Processes a block of BLE upgrade data.
 *
 * This function handles the processing of a BLE upgrade data block,
 * performing necessary operations to support the upgrade process.
 *
 * @param[in] p_data_block Pointer to the data block that contains upgrade data.
 * @param[in] u_data_block_len Length (in bytes) of the provided data block.
 *
 * @return An OPERATE_RET value indicating whether the processing was successful or if an error occurred.
 */
OPERATE_RET tuya_ble_upgrade_data_proc(BYTE_T *p_data_block, UINT_T u_data_block_len);

/**
 * @brief De-initializes the BLE upgrade service.
 *
 * This function releases all resources associated with the BLE upgrade process.
 * It should be called when BLE upgrade operations are complete or no longer necessary.
 *
 * @note Ensure that no upgrade transactions are in progress before calling this function.
 */
void tuya_ble_upgrade_deinit(void);
#ifdef __cplusplus
}
#endif 
#endif // _TUYA_BLE_UPGRADE_H