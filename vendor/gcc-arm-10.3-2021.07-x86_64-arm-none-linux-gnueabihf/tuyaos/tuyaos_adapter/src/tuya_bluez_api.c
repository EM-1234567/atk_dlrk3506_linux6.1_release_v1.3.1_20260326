#include <glib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tuya_bluez_compat.h"

#include "tuya_bluez_api.h"
#include "tuya_adv.h"
#include "tuya_hci.h"
#include "tuya_gatt.h"

static int g_bluez_inited = FALSE;
static GMainLoop *main_loop;

/**
 * @brief Dump host BLE/DBus environment into ble_diag log
 * @param[in] tag caller context string
 * @return none
 */
static void __ble_env_dump(const char *tag)
{
    char line[256];
    FILE *fp = NULL;
    int n = 0;

    PR_INFO("==== BLE ENV DUMP begin (%s) ====", tag ? tag : "-");

    PR_INFO("dbus socket: %s",
            (access("/run/dbus/system_bus_socket", F_OK) == 0) ? "EXISTS" : "MISSING");
    PR_INFO("machine-id: %s",
            (access("/var/lib/dbus/machine-id", R_OK) == 0) ? "EXISTS" : "MISSING");

    fp = fopen("/var/lib/dbus/machine-id", "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            PR_INFO("machine-id value: %s", line);
        }
        fclose(fp);
    }

    fp = popen("pidof dbus-daemon 2>/dev/null", "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            PR_INFO("dbus-daemon pid: %s", line);
        } else {
            PR_ERR("dbus-daemon: NOT RUNNING");
        }
        pclose(fp);
    }

    fp = popen("pidof bluetoothd 2>/dev/null", "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            PR_INFO("bluetoothd pid: %s", line);
        } else {
            PR_ERR("bluetoothd: NOT RUNNING");
        }
        pclose(fp);
    }

    PR_INFO("hci0 sysfs: %s",
            (access("/sys/class/bluetooth/hci0", F_OK) == 0) ? "EXISTS" : "MISSING");

    fp = popen("hciconfig hci0 2>/dev/null | head -6", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL && n < 6) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '\0') {
                PR_INFO("hciconfig: %s", line);
            }
            n++;
        }
        pclose(fp);
    }

    PR_INFO("adv_manager_ready: %d", tuya_adv_manager_ready() ? 1 : 0);

    /* Probe BlueZ Adapter1 / LEAdvertisingManager1 on the bus (async-safe enough for diag). */
    fp = popen("dbus-send --system --print-reply --dest=org.bluez "
               "/org/bluez/hci0 org.freedesktop.DBus.Introspectable.Introspect 2>/dev/null "
               "| tr '<>' '\\n' | grep -E 'LEAdvertising|Adapter1|GattManager' | head -12",
               "r");
    if (fp != NULL) {
        n = 0;
        while (fgets(line, sizeof(line), fp) != NULL && n < 12) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '\0') {
                PR_INFO("bluez iface: %s", line);
            }
            n++;
        }
        if (n == 0) {
            PR_WARN("bluez iface: (none /org/bluez/hci0 introspect empty)");
        }
        pclose(fp);
    }

    fp = popen("ps -o args= -C bluetoothd 2>/dev/null || "
               "tr '\\0' ' ' </proc/$(pidof bluetoothd)/cmdline 2>/dev/null",
               "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            PR_INFO("bluetoothd cmdline: %s", line);
        }
        pclose(fp);
    }

    PR_INFO("==== BLE ENV DUMP end (%s) ====", tag ? tag : "-");
}

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
        PR_INFO("tuya bluez already initialized");
        __ble_env_dump("re-init");
        return 0;
    }

    PR_INFO("tuya_bluez_init start");
    __ble_env_dump("pre-init");

    main_loop = g_main_loop_new(NULL, FALSE);

    ret = tuya_gatt_init();
    if (ret != 0) {
        PR_ERR("tuya_gatt_init error %d", ret);
        return ret;
    }
    PR_INFO("tuya_gatt_init OK");

    /* Advertising uses raw legacy HCI (tuya_hci), NOT the BlueZ D-Bus adv path.
     * Do NOT call tuya_adv_init / register a LEAdvertisement1 object -- that
     * path registers but the chip never airs it, and registering it would make
     * bluetoothd/mgmt claim advertising (which then rejects raw HCI 0x0C).
     * Keeping mgmt adv-free lets raw HCI LE Set Adv reach the controller. */
    /* ret = tuya_adv_init(tuya_gatt_get_connection()); */
    (void)ret;

    pthread_create(&tid, NULL, __loop_run, main_loop);
    PR_INFO("g_main_loop thread started");

    g_bluez_inited = TRUE;
    __ble_env_dump("post-init");

    return 0;
}

int tuya_bluez_deinit(void)
{
    return 0;
}

/*
 * TAL may issue adv_start / adv_data before (or without) GATT service
 * registration having triggered tuya_bluez_init(). In that case s_adv.conn is
 * NULL, the BlueZ D-Bus path is skipped, and we fall back to legacy HCI
 * LE Set Adv -- which the controller rejects with 0x0C (Command Disallowed)
 * while bluetoothd owns the adapter via MGMT. Force the BlueZ/D-Bus stack up
 * here so advertising always goes through LEAdvertisingManager1.
 */
static void __ensure_bluez_init(void)
{
    if (!g_bluez_inited) {
        PR_INFO("bluez not inited before adv op; lazy init now");
        (void)tuya_bluez_init();
    }
}

/*
 * Advertising via raw legacy HCI -- the ORIGINAL/working path (this is what
 * transmitted when the device was scannable). The BlueZ D-Bus
 * LEAdvertisingManager1 path (tuya_adv.c) registers the adv but the chip never
 * airs it, so do NOT use it. Raw HCI LE Set Adv goes straight to the controller.
 * Because we don't register a BlueZ advertisement, bluetoothd/mgmt has no adv
 * instance, so there is no MGMT conflict -> raw HCI adv is NOT rejected 0x0C.
 * GATT still uses BlueZ D-Bus (tuya_gatt_*).
 */
int tuya_bluez_le_set_adv_params(le_set_adv_params_t *params)
{
    if (params == NULL) {
        return 1;
    }
    /* Disable first, then try Set Adv Params. If mgmt locks it (0x12), ignore
     * the failure and use bluetoothd's default params. Return 0 so the caller
     * continues to Set Data + Enable. DO NOT skip the HCI call entirely — the
     * retry delay preserves timing that avoids the gdbus race crash. */
    tuya_hci_le_set_adv_enable(false);
    usleep(50000);
    int ret = tuya_hci_le_set_adv_params(params->min_interval, params->max_interval, params->advtype);
    if (ret != 0) {
        PR_WARN("adv: Set Adv Params failed (%d), using default params", ret);
    }
    return 0;
}

int tuya_bluez_le_set_adv_enable(bool enable)
{
    PR_INFO("=== adv via RAW HCI (legacy) enable=%d ===", enable);
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
    int i = 0, j = 0;
    uint16_t svc_uuid = 0x0000;
    le_gatt_characteristic_t *p_chr = NULL;
    int registered = 0;

    printf("register gatt servicenum: %u\n", service_num);
    PR_INFO("register gatt servicenum: %u", service_num);

    if ((service == NULL) || (service_num == 0)) {
        return 1;
    }

    /*
     * TAL calls gatts_service_add BEFORE tkl_ble_stack_init.
     * BlueZ/DBus must be ready first, otherwise every register fails and
     * tal_ble_bt_init maps that to OPRT_OS_ADAPTER_BLE_INIT_FAILED
     * (0xffff8ffa) — stack never inits and advertising never starts.
     */
    if (tuya_bluez_init() != 0) {
        printf("tuya_bluez_init error before gatt register\n");
        return 1;
    }

    for (i = 0; i < service_num; i++) {
        svc_uuid = service[i].uuid;
        printf("svc_uuid:%04x\n", svc_uuid);
        if (tuya_gatt_register_service(svc_uuid) != 0) {
            printf("tuya_gatt_register_service error\n");
            continue;
        }
        p_chr = service[i].chr;
        for (j = 0; j < service[i].chr_num; j++) {
            uint16_t desc_uuid = 0;
            uint8_t desc_props = 0;

            printf("p_chr[%d].uuid:%s\n", j, p_chr[j].uuid);
            printf("p_chr[%d].property:%u\n", j, p_chr[j].property);
            if ((p_chr[j].property & LE_GATT_CHR_PROP_NOTIFY) ||
                (p_chr[j].property & LE_GATT_CHR_PROP_INDICATE)) {
                /* CCCD required by many phone stacks before StartNotify/write flow */
                desc_uuid = 0x2902;
                desc_props = LE_GATT_CHR_PROP_READ | LE_GATT_CHR_PROP_WRITE;
            }
            if (tuya_gatt_register_characteristic(svc_uuid, p_chr[j].uuid, p_chr[j].property,
                                                  desc_uuid, desc_props) != 0) {
                printf("tuya_gatt_register_characteristic error\n");
                break;
            }
        }
        registered++;
    }

    /* Register with BlueZ only after local objects exist (avoids "No object received"). */
    if (registered > 0) {
        tuya_gatt_register_application();
    }

    return (registered > 0) ? 0 : 1;
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
void tuya_bluez_le_register_write_req_event(void (*cb)(uint16_t uuid, uint8_t *data, uint16_t len))
{
    tuya_gatt_register_write_req_event(cb);
}
