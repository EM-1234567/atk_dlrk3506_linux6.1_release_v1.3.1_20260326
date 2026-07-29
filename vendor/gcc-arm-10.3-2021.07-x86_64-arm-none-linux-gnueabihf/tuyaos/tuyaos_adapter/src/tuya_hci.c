/**
 * @file tuya_hci.c
 * @brief Legacy HCI LE advertising helpers for BlueZ adapter
 * @version 1.0
 * @date 2026-07-24
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_hci.h"
#include <stdio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "tuya_bluez_compat.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define HCI_LE_ADV_DATA_MAX_LEN 31
#define HCI_CMD_TIMEOUT_MS      1000

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Map HCI command status to adapter error code
 * @param[in] status HCI Command Complete status byte
 * @return LE_SUCCESS or LE_HCI_STATUS_ERROR
 */
static int __hci_status_to_ret(uint8_t status)
{
    if (status == 0x00) {
        return LE_SUCCESS;
    }
    return LE_HCI_STATUS_ERROR;
}

/*
 * Map Tuya TKL adv_type encoding to Bluetooth HCI Adv_Type field.
 * TKL and HCI use DIFFERENT encodings — direct pass-through causes
 * CONN_SCANNABLE_UNDIRECTED (TKL 0x01) to become ADV_DIRECT_IND (HCI 0x01),
 * making the advertisement invisible to all scanners (directed to zero addr).
 */
static uint8_t __tkl_to_hci_adv_type(uint8_t tkl_type)
{
    switch (tkl_type) {
    case 0x01: return 0x00; /* CONN_SCANNABLE_UNDIRECTED  -> ADV_IND (connectable+scannable+undirected) */
    case 0x02: return 0x01; /* CONN_NONSCANNABLE_DIR_HIGH -> ADV_DIRECT_IND (high duty directed) */
    case 0x03: return 0x04; /* CONN_NONSCANNABLE_DIR_LOW  -> ADV_DIRECT_IND_LOW (low duty directed) */
    case 0x04: return 0x02; /* NONCONN_SCANNABLE_UNDIR    -> ADV_SCAN_IND (scannable undirected) */
    case 0x05: return 0x03; /* NONCONN_NONSCANNABLE_UNDIR -> ADV_NONCONN_IND (non-connectable undirected) */
    default:   return 0x00; /* unknown -> ADV_IND (safest, scannable by all) */
    }
}

/**
 * @brief Set LE advertising parameters via legacy HCI
 * @param[in] min_interval minimum advertising interval
 * @param[in] max_interval maximum advertising interval
 * @param[in] advtype advertising type
 * @return LE_SUCCESS on success, error code on failure
 */
int tuya_hci_le_set_adv_params(uint16_t min_interval, uint16_t max_interval, uint8_t advtype)
{
    int device = 0;
    uint8_t status = 0;
    int ret = LE_SUCCESS;
    le_set_advertising_parameters_cp adv_params_cp;
    struct hci_request req;

    { int _rt = hci_get_route(NULL); device = hci_open_dev(_rt); PR_INFO("hci: open route=%d device=%d", _rt, device); }
    if (device < 0) {
        PR_ERR("hci: open device FAILED");
        return LE_OPEN_ERROR;
    }

    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.advtype = __tkl_to_hci_adv_type(advtype);
    adv_params_cp.min_interval = htobs(min_interval);
    adv_params_cp.max_interval = htobs(max_interval);
    adv_params_cp.chan_map = 7;

    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
    req.cparam = &adv_params_cp;
    req.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
    req.rparam = &status;
    req.rlen = 1;

    if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
        /* Controller Busy (0x12) or send failure — retry a few times. */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            PR_WARN("hci: retry %d/3 (prev send failed)", _retry + 1);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                break;
            }
        }
        if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
            hci_close_dev(device);
            return LE_READ_ERROR;
        }
    }
    ret = __hci_status_to_ret(status);
    if (ret != LE_SUCCESS) {
        /* Retry on non-zero status (0x12 Controller Busy etc). */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                ret = __hci_status_to_ret(status);
                if (ret == LE_SUCCESS) {
                    break;
                }
            }
            PR_WARN("hci: retry %d/3 (status=0x%02x)", _retry + 1, status);
        }
    }
    PR_INFO("hci: LE Set Adv Params status=0x%02x (%s) ret=%d [type=%d min=%d max=%d]", status,
            status == 0 ? "OK" : (status == 0x0C ? "DISALLOWED" : "ERR"), ret, advtype, min_interval, max_interval);
    hci_close_dev(device);
    return ret;
}

/**
 * @brief Enable or disable LE advertising via legacy HCI
 * @param[in] enable true to enable, false to disable
 * @return LE_SUCCESS on success, error code on failure
 */
int tuya_hci_le_set_adv_enable(bool enable)
{
    int device = 0;
    uint8_t status = 0;
    int ret = LE_SUCCESS;
    le_set_advertise_enable_cp advertise_cp;
    struct hci_request req;

    { int _rt = hci_get_route(NULL); device = hci_open_dev(_rt); PR_INFO("hci: open route=%d device=%d", _rt, device); }
    if (device < 0) {
        PR_ERR("hci: open device FAILED");
        return LE_OPEN_ERROR;
    }

    memset(&advertise_cp, 0, sizeof(advertise_cp));
    advertise_cp.enable = enable;

    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    req.cparam = &advertise_cp;
    req.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    req.rparam = &status;
    req.rlen = 1;

    if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
        /* Controller Busy (0x12) or send failure — retry a few times. */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            PR_WARN("hci: retry %d/3 (prev send failed)", _retry + 1);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                break;
            }
        }
        if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
            hci_close_dev(device);
            return LE_READ_ERROR;
        }
    }
    ret = __hci_status_to_ret(status);
    if (ret != LE_SUCCESS) {
        /* Retry on non-zero status (0x12 Controller Busy etc). */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                ret = __hci_status_to_ret(status);
                if (ret == LE_SUCCESS) {
                    break;
                }
            }
            PR_WARN("hci: retry %d/3 (status=0x%02x)", _retry + 1, status);
        }
    }
    PR_INFO("hci: LE Set Adv %s status=0x%02x (%s) ret=%d", enable ? "Enable" : "Disable", status,
            status == 0 ? "OK" : (status == 0x0C ? "DISALLOWED" : "ERR"), ret);
    hci_close_dev(device);
    return ret;
}

/**
 * @brief Set LE advertising data via legacy HCI
 * @param[in] data advertising payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success, error code on failure
 */
int tuya_hci_le_set_adv_data(uint8_t *data, uint8_t len)
{
    int device = 0;
    uint8_t status = 0;
    int ret = LE_SUCCESS;
    int i = 0;
    le_set_advertising_data_cp adv_data_cp;
    struct hci_request req;

    if ((data == NULL) || (len == 0) || (len > HCI_LE_ADV_DATA_MAX_LEN)) {
        return LE_INVALID_PARAM;
    }

    { int _rt = hci_get_route(NULL); device = hci_open_dev(_rt); PR_INFO("hci: open route=%d device=%d", _rt, device); }
    if (device < 0) {
        PR_ERR("hci: open device FAILED");
        return LE_OPEN_ERROR;
    }

    for (i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    memset(&adv_data_cp, 0, sizeof(adv_data_cp));
    memcpy(adv_data_cp.data, data, len);
    adv_data_cp.length = len;

    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_ADVERTISING_DATA;
    req.cparam = &adv_data_cp;
    req.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
    req.rparam = &status;
    req.rlen = 1;

    if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
        /* Controller Busy (0x12) or send failure — retry a few times. */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            PR_WARN("hci: retry %d/3 (prev send failed)", _retry + 1);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                break;
            }
        }
        if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
            hci_close_dev(device);
            return LE_READ_ERROR;
        }
    }
    ret = __hci_status_to_ret(status);
    if (ret != LE_SUCCESS) {
        /* Retry on non-zero status (0x12 Controller Busy etc). */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                ret = __hci_status_to_ret(status);
                if (ret == LE_SUCCESS) {
                    break;
                }
            }
            PR_WARN("hci: retry %d/3 (status=0x%02x)", _retry + 1, status);
        }
    }
    PR_INFO("hci: LE Set Adv Data status=0x%02x (%s) ret=%d len=%d", status,
            status == 0 ? "OK" : (status == 0x0C ? "DISALLOWED" : "ERR"), ret, len);
    hci_close_dev(device);
    return ret;
}

/**
 * @brief Set LE scan response data via legacy HCI
 * @param[in] data scan response payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success, error code on failure
 */
int tuya_hci_le_set_scan_rsp_data(uint8_t *data, uint8_t len)
{
    int device = 0;
    uint8_t status = 0;
    int ret = LE_SUCCESS;
    int i = 0;
    le_set_scan_response_data_cp scan_rsp_data_cp;
    struct hci_request req;

    if ((data == NULL) || (len == 0) || (len > HCI_LE_ADV_DATA_MAX_LEN)) {
        return LE_INVALID_PARAM;
    }

    { int _rt = hci_get_route(NULL); device = hci_open_dev(_rt); PR_INFO("hci: open route=%d device=%d", _rt, device); }
    if (device < 0) {
        PR_ERR("hci: open device FAILED");
        return LE_OPEN_ERROR;
    }

    for (i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    memset(&scan_rsp_data_cp, 0, sizeof(scan_rsp_data_cp));
    memcpy(scan_rsp_data_cp.data, data, len);
    scan_rsp_data_cp.length = len;

    memset(&req, 0, sizeof(req));
    req.ogf = OGF_LE_CTL;
    req.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
    req.cparam = &scan_rsp_data_cp;
    req.clen = LE_SET_SCAN_RESPONSE_DATA_CP_SIZE;
    req.rparam = &status;
    req.rlen = 1;

    if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
        /* Controller Busy (0x12) or send failure — retry a few times. */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            PR_WARN("hci: retry %d/3 (prev send failed)", _retry + 1);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                break;
            }
        }
        if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) != 0) {
            hci_close_dev(device);
            return LE_READ_ERROR;
        }
    }
    ret = __hci_status_to_ret(status);
    if (ret != LE_SUCCESS) {
        /* Retry on non-zero status (0x12 Controller Busy etc). */
        int _retry;
        for (_retry = 0; _retry < 10; _retry++) {
            usleep(300000);
            if (hci_send_req(device, &req, HCI_CMD_TIMEOUT_MS) == 0) {
                ret = __hci_status_to_ret(status);
                if (ret == LE_SUCCESS) {
                    break;
                }
            }
            PR_WARN("hci: retry %d/3 (status=0x%02x)", _retry + 1, status);
        }
    }
    PR_INFO("hci: LE Set Scan Rsp status=0x%02x (%s) ret=%d len=%d", status,
            status == 0 ? "OK" : (status == 0x0C ? "DISALLOWED" : "ERR"), ret, len);
    hci_close_dev(device);
    return ret;
}
