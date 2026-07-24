/**
 * @file tkl_wired.c
 * @brief Wired TKL stub for Wi-Fi-only SoftAP product (RK3506 + RTL8733BU)
 * @version 1.0
 * @date 2026-07-22
 * @copyright Copyright (c) Tuya Inc.
 *
 * @note Default Linux stub always reported LINK_UP + fake IP 192.168.31.168.
 *       That made netmgr flip active linkage to "wired" after every Wi-Fi TLS
 *       failure, burning activate retries on a dead NIC (tcp -0x7108).
 *       This board provisions and activates over Wi-Fi only — keep wired DOWN.
 */
#include "tuya_cloud_types.h"
#include "tkl_wired.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Get wired link status (always down on this product)
 * @param[out] status wired link status
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_wired_get_status(TKL_WIRED_STAT_E *status)
{
    if (status == NULL) {
        return OPRT_INVALID_PARM;
    }
    *status = TKL_WIRED_LINK_DOWN;
    return OPRT_OK;
}

/**
 * @brief Register wired status callback; report DOWN once
 * @param[in] cb status change callback
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_wired_set_status_cb(TKL_WIRED_STATUS_CHANGE_CB cb)
{
    if (cb) {
        cb(TKL_WIRED_LINK_DOWN);
    }
    return OPRT_OK;
}

/**
 * @brief Get wired IP (not available)
 * @param[out] ip IP buffer
 * @return OPRT_COM_ERROR — no wired interface in use
 */
OPERATE_RET tkl_wired_get_ip(NW_IP_S *ip)
{
    if (ip == NULL) {
        return OPRT_INVALID_PARM;
    }
    memset(ip, 0, sizeof(NW_IP_S));
    return OPRT_COM_ERROR;
}

/**
 * @brief Get wired MAC (zeros)
 * @param[out] mac MAC buffer
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_wired_get_mac(NW_MAC_S *mac)
{
    if (mac == NULL) {
        return OPRT_INVALID_PARM;
    }
    memset(mac, 0, sizeof(NW_MAC_S));
    return OPRT_OK;
}

/**
 * @brief Set wired MAC (no-op)
 * @param[in] mac MAC to set
 * @return OPRT_OK
 */
OPERATE_RET tkl_wired_set_mac(CONST NW_MAC_S *mac)
{
    (void)mac;
    return OPRT_OK;
}
