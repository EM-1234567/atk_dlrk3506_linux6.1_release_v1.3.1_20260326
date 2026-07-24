#include <glib.h>
#include <pthread.h>

#include "tuya_bluez_compat.h"

#include "tuya_bluez_api.h"
#include "tuya_hci.h"
#include "tuya_gatt.h"
#include <stdio.h>
static int g_bluez_inited = FALSE;
static GMainLoop *main_loop;

static void *__loop_run(void *arg)
{
    g_main_loop_run((GMainLoop *)arg);

    return NULL;
}

int tuya_bluez_init(void)
{
    int ret = 0;
    pthread_t tid;

    if (g_bluez_inited) {
        printf("tuya bluez had been initialized");
        return 0;
    }

    main_loop = g_main_loop_new(NULL, FALSE);

    ret = tuya_gatt_init();
    if (ret != 0) {
       printf("tuya_gatt_init error");
        return ret;
    }

    pthread_create(&tid, NULL, __loop_run, main_loop);

    g_bluez_inited = TRUE;

    return 0;
}

int tuya_bluez_deinit(void)
{
    return 0;
}

int tuya_bluez_le_set_adv_params(le_set_adv_params_t *params)
{
    if (params == NULL) {
        return 1;
    }

    return tuya_hci_le_set_adv_params(params->min_interval, params->max_interval, params->advtype);
}

int tuya_bluez_le_set_adv_enable(bool enable)
{
    return tuya_hci_le_set_adv_enable(enable);
}

int tuya_bluez_le_set_adv_data(uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len == 0)) {
        return 1;
    }

    return tuya_hci_le_set_adv_data(data, len);
}

int tuya_bluez_le_set_scan_rsp_data(uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len == 0)) {
        return 1;
    }

    return tuya_hci_le_set_scan_rsp_data(data, len);
}

int tuya_bluez_le_add_gatt_service(le_gatt_service_t *service, uint8_t service_num)
{
    printf("register gatt servicenum: %u\n", service_num);
   
    

    if ((service == NULL) || (service_num == 0)) {
        return 1;
    }

    int i = 0, j = 0;
    uint16_t svc_uuid = 0x0000;
    le_gatt_characteristic_t *p_chr = NULL;

    for (i = 0; i < service_num; i++) {
        svc_uuid = service[i].uuid;
        printf("svc_uuid:%04x\n",svc_uuid);
        if (tuya_gatt_register_service(svc_uuid) != 0) {
            printf("tuya_gatt_register_service error");
            continue;
        }
        p_chr = service[i].chr;
        for (j = 0; j < service[i].chr_num; j++) {
            printf("p_chr[%d].uuid:%s\n",j,p_chr[j].uuid);
            printf("p_chr[%d].property:%u\n",j,p_chr[j].property);
            if (tuya_gatt_register_characteristic(svc_uuid, p_chr[j].uuid, p_chr[j].property, 0 , 0) != 0) {
               printf("tuya_gatt_register_characteristic error");
                break;
            }
        }
    }

    return 0;
}

int tuya_bluez_le_gatts_value_notify(uint16_t uuid, uint8_t *value, uint16_t len)
{
    return tuya_gatt_server_send_characteristic_notification(uuid, value, len);
}

void tuya_bluez_le_register_connect_event(void(*cb)(int status))
{
    tuya_gatt_register_connect_event(cb);
}

/**
 * @brief Register write request event callback
 */
void tuya_bluez_le_register_write_req_event(void(*cb)(uint16_t uuid, uint8_t *data, uint8_t len))
{
    tuya_gatt_register_write_req_event(cb);
}
