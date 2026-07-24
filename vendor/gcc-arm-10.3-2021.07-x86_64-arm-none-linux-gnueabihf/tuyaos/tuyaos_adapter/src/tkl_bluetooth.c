#include "tkl_bluetooth.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "tuya_cloud_types.h"
#include "tuya_bluez_api.h"
#include "tuya_bluez_compat.h"

#define BLE_CONN_HANDLE 0xFD50

typedef struct {
    UINT16_T uuid;
    USHORT_T length;
    UCHAR_T data[0];
} BLE_CACHE_DATA_T;

STATIC TKL_BLE_GAP_EVT_FUNC_CB __gap_evt_cb   = NULL;
STATIC TKL_BLE_GATT_EVT_FUNC_CB __gatt_evt_cb = NULL;

STATIC BOOL_T g_connected          = FALSE;
STATIC P_QUEUE_CLASS g_cache_queue = NULL;
static pthread_mutex_t g_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static USHORT_T tuya_conn_handle = 0;

STATIC VOID __gatt_write_request_event_cb(UINT16_T uuid, UINT8_T *data, UINT8_T len)
{
    BLE_CACHE_DATA_T *cache_data = NULL;

    PR_DEBUG("recv write request, uuid: 0x%04x, len: %d", uuid, len);

    /**
     * BlueZ uses D-BUS for inter-process communication. Data may arrive
     * before the connection event is detected. Cache data until connected.
     */
    if (!g_connected) {
        pthread_mutex_lock(&g_cache_mutex);
        cache_data = (BLE_CACHE_DATA_T *)Malloc(SIZEOF(BLE_CACHE_DATA_T) + len);
        if (!cache_data) {
            PR_ERR("Malloc err");
            pthread_mutex_unlock(&g_cache_mutex);
            return;
        }
        cache_data->uuid   = uuid;
        cache_data->length = len;
        memcpy(cache_data->data, data, len);
        InQueue(g_cache_queue, (unsigned char *)&cache_data, 1);
        pthread_mutex_unlock(&g_cache_mutex);
        return;
    }

    TKL_BLE_GATT_PARAMS_EVT_T event;
    memset(&event, 0, SIZEOF(TKL_BLE_GATT_PARAMS_EVT_T));

    event.result                                = 0;
    event.type                                  = TKL_BLE_GATT_EVT_WRITE_REQ;
    event.conn_handle                           = tuya_conn_handle;
    event.gatt_event.write_report.char_handle   = uuid;
    event.gatt_event.write_report.report.p_data = data;
    event.gatt_event.write_report.report.length = len;

    if (__gatt_evt_cb)
        __gatt_evt_cb(&event);
}

STATIC VOID __gap_connect_event_cb(INT_T status)
{
    PR_INFO("recv connect event, status: %d", status);
    g_connected = status;

    TKL_BLE_GAP_PARAMS_EVT_T event;
    memset(&event, 0, SIZEOF(TKL_BLE_GAP_PARAMS_EVT_T));

    event.result = 0;
    if (status) {
        event.type = TKL_BLE_GAP_EVT_CONNECT;
    } else {
        event.type = TKL_BLE_GAP_EVT_DISCONNECT;
    }
    event.conn_handle            = tuya_conn_handle;
    event.gap_event.connect.role = TKL_BLE_ROLE_SERVER;

    if (__gap_evt_cb) {
        __gap_evt_cb(&event);
    }

    /**
     * BlueZ D-BUS race: data may arrive before connection event.
     * Flush cached data after connection is established.
     */
    pthread_mutex_lock(&g_cache_mutex);
    while (GetCurQueNum(g_cache_queue)) {
        BLE_CACHE_DATA_T *cache_data = NULL;
        if (!OutQueue(g_cache_queue, (unsigned char *)&cache_data, 1)) {
            break;
        }
        if (cache_data == NULL) {
            break;
        }
        PR_DEBUG("flush cache: uuid=0x%04x, len=%d", cache_data->uuid, cache_data->length);
        __gatt_write_request_event_cb(cache_data->uuid, cache_data->data, cache_data->length);
        Free(cache_data);
        cache_data = NULL;
    }
    pthread_mutex_unlock(&g_cache_mutex);
}

/**
 * Notify SDK that BLE stack initialization is complete.
 * Without this, SDK will not start advertising.
 */
STATIC VOID __gap_init_event_cb(VOID)
{
    TKL_BLE_GAP_PARAMS_EVT_T event;
    memset(&event, 0, SIZEOF(TKL_BLE_GAP_PARAMS_EVT_T));

    event.result                 = 0;
    event.type                   = TKL_BLE_EVT_STACK_INIT;
    event.conn_handle            = tuya_conn_handle;
    event.gap_event.connect.role = TKL_BLE_ROLE_SERVER;

    if (__gap_evt_cb) {
        __gap_evt_cb(&event);
    }
}

OPERATE_RET tkl_ble_stack_init(UCHAR_T role)
{
    PR_INFO("tkl_ble_stack_init");

    g_cache_queue = CreateQueueObj(32, SIZEOF(BLE_CACHE_DATA_T));
    if (!g_cache_queue) {
        PR_ERR("CreateQueueObj error");
        return OPRT_COM_ERROR;
    }

    tuya_bluez_init();
    tuya_bluez_le_register_connect_event(__gap_connect_event_cb);
    tuya_bluez_le_register_write_req_event(__gatt_write_request_event_cb);

    __gap_init_event_cb();

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_callback_register(CONST TKL_BLE_GAP_EVT_FUNC_CB gap_evt)
{
    PR_INFO("tkl_ble_gap_callback_register");
    __gap_evt_cb = gap_evt;
    return OPRT_OK;
}

OPERATE_RET tkl_ble_gatt_callback_register(CONST TKL_BLE_GATT_EVT_FUNC_CB gatt_evt)
{
    PR_INFO("tkl_ble_gatt_callback_register");
    __gatt_evt_cb = gatt_evt;
    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_adv_start(TKL_BLE_GAP_ADV_PARAMS_T CONST *p_adv_params)
{
    PR_INFO("tkl_ble_gap_adv_start");

    if (p_adv_params == NULL) {
        return OPRT_INVALID_PARM;
    }

    /*
     * BLE coexists with SoftAP on RTL8733BU: BLE is an independent netcfg
     * channel (driven by tuya_enable_ble_netcfg), while WF_START_AP_ONLY keeps
     * WiFi provisioning on SoftAP (SmartLife-xxxx). WiFi(SDIO) and BT(UART) are
     * separate buses, so both can run in parallel; the App picks either path.
     */
    le_set_adv_params_t adv_param;
    memset(&adv_param, 0, sizeof(adv_param));
    adv_param.advtype      = p_adv_params->adv_type;
    adv_param.min_interval = p_adv_params->adv_interval_min;
    adv_param.max_interval = p_adv_params->adv_interval_max;

    tuya_bluez_le_set_adv_params(&adv_param);
    tuya_bluez_le_set_adv_enable(1);

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_adv_stop(VOID)
{
    PR_INFO("tkl_ble_gap_adv_stop");
    tuya_bluez_le_set_adv_enable(0);
    return OPRT_OK;
}

/**
 * @brief Pick a 16-bit handle used by BlueZ write/notify callback matching
 * @param[in] uuid TKL UUID
 * @return 16-bit handle value
 */
STATIC USHORT_T __tkl_uuid_to_handle(CONST TKL_BLE_UUID_T *uuid)
{
    if (uuid == NULL) {
        return 0;
    }
    if (uuid->uuid_type == TKL_BLE_UUID_TYPE_16) {
        return uuid->uuid.uuid16;
    }
    if (uuid->uuid_type == TKL_BLE_UUID_TYPE_32) {
        return (USHORT_T)(uuid->uuid.uuid32 & 0xFFFF);
    }
    /* LE uuid128: bytes[1]<<8 | bytes[0] commonly holds the 16-bit alias */
    return (USHORT_T)(uuid->uuid.uuid128[0] | ((USHORT_T)uuid->uuid.uuid128[1] << 8));
}

OPERATE_RET tkl_ble_gap_adv_rsp_data_set(TKL_BLE_DATA_T CONST *p_adv, TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    PR_INFO("tkl_ble_gap_adv_rsp_data_set");
    if ((p_adv == NULL) || (p_scan_rsp == NULL) || (p_adv->p_data == NULL) || (p_scan_rsp->p_data == NULL)) {
        return OPRT_INVALID_PARM;
    }
    tuya_bluez_le_set_adv_data(p_adv->p_data, p_adv->length);
    tuya_bluez_le_set_scan_rsp_data(p_scan_rsp->p_data, p_scan_rsp->length);
    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_adv_rsp_data_update(TKL_BLE_DATA_T CONST *p_adv, TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    PR_INFO("tkl_ble_gap_adv_rsp_data_update");
    return tkl_ble_gap_adv_rsp_data_set(p_adv, p_scan_rsp);
}

OPERATE_RET tkl_ble_gatts_service_add(TKL_BLE_GATTS_PARAMS_T *p_service)
{
    INT_T i = 0, j = 0;
    UINT8_T svc_num             = 0;
    le_gatt_service_t *gatt_svc = NULL;

    if ((p_service == NULL) || (p_service->p_service == NULL) || (p_service->svc_num == 0)) {
        return OPRT_INVALID_PARM;
    }

    svc_num = p_service->svc_num;
    PR_INFO("register gatt service, num: %u", svc_num);

    gatt_svc = (le_gatt_service_t *)Malloc(svc_num * SIZEOF(le_gatt_service_t));
    if (!gatt_svc) {
        return OPRT_MALLOC_FAILED;
    }
    memset(gatt_svc, 0, svc_num * SIZEOF(le_gatt_service_t));

    for (i = 0; i < svc_num; i++) {
        TKL_BLE_SERVICE_PARAMS_T *p_service_param = &p_service->p_service[i];

        gatt_svc[i].uuid    = p_service_param->svc_uuid.uuid.uuid16;
        gatt_svc[i].type    = p_service_param->type;
        gatt_svc[i].chr_num = p_service_param->char_num;
        p_service_param->handle = p_service_param->svc_uuid.uuid.uuid16;
        tuya_conn_handle = p_service_param->handle;

        PR_DEBUG("service uuid: 0x%04x, char_num: %u", gatt_svc[i].uuid, gatt_svc[i].chr_num);

        UINT8_T chr_num                    = p_service_param->char_num;
        le_gatt_characteristic_t *gatt_chr = (le_gatt_characteristic_t *)Malloc(chr_num * SIZEOF(le_gatt_characteristic_t));
        if (!gatt_chr) {
            Free(gatt_svc);
            return OPRT_MALLOC_FAILED;
        }
        memset(gatt_chr, 0, chr_num * SIZEOF(le_gatt_characteristic_t));

        for (j = 0; j < gatt_svc[i].chr_num; j++) {
            USHORT_T handle = __tkl_uuid_to_handle(&p_service_param->p_char[j].char_uuid);

            /*
             * BlueZ write/notify match uses strtol(uuid_str, 16).
             * Always publish the 16-bit handle form so callbacks stay consistent.
             */
            snprintf((CHAR_T *)gatt_chr[j].uuid, sizeof(gatt_chr[j].uuid), "%04x", handle);
            gatt_chr[j].property = p_service_param->p_char[j].property;
            p_service_param->p_char[j].handle = handle;
            PR_DEBUG("chr[%d] uuid: %s, handle: 0x%04x", j, gatt_chr[j].uuid, handle);
        }

        gatt_svc[i].chr = gatt_chr;
    }

    tuya_bluez_le_add_gatt_service(gatt_svc, svc_num);

    for (i = 0; i < svc_num; i++) {
        if (gatt_svc[i].chr) {
            Free(gatt_svc[i].chr);
        }
    }
    Free(gatt_svc);

    return OPRT_OK;
}



OPERATE_RET tkl_ble_gatts_value_notify(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{
    PR_DEBUG("tkl_ble_gatts_value_notify, chr_handle: 0x%04x", char_handle);
    tuya_bluez_le_gatts_value_notify(char_handle, p_data, length);
    return OPRT_OK;
}

OPERATE_RET tkl_ble_stack_deinit(UCHAR_T role)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_stack_gatt_link(USHORT_T *p_link)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_addr_set(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_address_get(TKL_BLE_GAP_ADDR_T *p_peer_addr)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_scan_start(TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_scan_stop(VOID)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_connect(TKL_BLE_GAP_ADDR_T CONST *p_peer_addr, TKL_BLE_GAP_SCAN_PARAMS_T CONST *p_scan_params, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_disconnect(USHORT_T conn_handle, UCHAR_T hci_reason)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_conn_param_update(USHORT_T conn_handle, TKL_BLE_GAP_CONN_PARAMS_T CONST *p_conn_params)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_tx_power_set(UCHAR_T role, INT_T tx_power)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_rssi_get(USHORT_T conn_handle)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gap_name_set(CHAR_T *p_name)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gatts_value_set(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gatts_value_get(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gatts_value_indicate(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gatts_exchange_mtu_reply(USHORT_T conn_handle, USHORT_T server_rx_mtu)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_all_service_discovery(USHORT_T conn_handle)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_all_char_discovery(USHORT_T conn_handle, USHORT_T start_handle, USHORT_T end_handle)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_char_desc_discovery(USHORT_T conn_handle, USHORT_T start_handle, USHORT_T end_handle)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_write_without_rsp(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_write(USHORT_T conn_handle, USHORT_T char_handle, UCHAR_T *p_data, USHORT_T length)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_read(USHORT_T conn_handle, USHORT_T char_handle)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_gattc_exchange_mtu_request(USHORT_T conn_handle, USHORT_T client_rx_mtu)
{

    return OPRT_OK;
}

OPERATE_RET tkl_ble_vendor_command_control(USHORT_T opcode, VOID_T *user_data, USHORT_T data_len)
{

    return OPRT_NOT_SUPPORTED;
}

void rend_bt_data(uint8_t len, uint8_t *buffer)
{
}


/* ---------------------------------------------------------------------------
 * Stubs required by public TKL header (peripheral provisioning path unused)
 * --------------------------------------------------------------------------- */
OPERATE_RET tkl_ble_gatts_service_change(USHORT_T conn_handle, USHORT_T start_handle, USHORT_T end_handle)
{
    (void)conn_handle;
    (void)start_handle;
    (void)end_handle;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_create(TKL_BLE_GAP_EXT_ADV_T *p_ext_adv)
{
    (void)p_ext_adv;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_config(TKL_BLE_GAP_EXT_ADV_T ext_adv, TKL_BLE_GAP_EXT_ADV_PARAMS_T CONST *p_adv_params,
                                       TKL_BLE_DATA_T CONST *p_adv_data, TKL_BLE_DATA_T CONST *p_scan_rsp)
{
    (void)ext_adv;
    (void)p_adv_params;
    (void)p_adv_data;
    (void)p_scan_rsp;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_start(TKL_BLE_GAP_EXT_ADV_T ext_adv)
{
    (void)ext_adv;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_stop(TKL_BLE_GAP_EXT_ADV_T ext_adv)
{
    (void)ext_adv;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_delete(TKL_BLE_GAP_EXT_ADV_T ext_adv)
{
    (void)ext_adv;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tkl_ble_gap_ext_adv_clear(void)
{
    return OPRT_NOT_SUPPORTED;
}

uint16_t tkl_ble_gap_ext_adv_get_max_data_length(void)
{
    return 0;
}

uint8_t tkl_ble_gap_ext_adv_get_support_number(void)
{
    return 0;
}

OPERATE_RET tkl_ble_set_mode(CONST BOOL_T enable, CONST UCHAR_T mode)
{
    (void)enable;
    (void)mode;
    return OPRT_OK;
}
