/**
 * @file tuya_adv.c
 * @brief BlueZ LEAdvertisingManager1 path (fixes legacy HCI 0x0C with bluetoothd)
 * @version 1.0
 * @date 2026-07-24
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_adv.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gdbus/gdbus.h"
#include "tuya_bluez_compat.h"
#include "tuya_bluez_def.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define LE_ADV_IFACE "org.bluez.LEAdvertisement1"
#define LE_ADV_MGR_IFACE "org.bluez.LEAdvertisingManager1"
#define ADV_PATH "/com/tuya/advertisement0"
#define ADV_DATA_MAX 31
#define ADV_SVC_UUID_MAX 4
#define ADV_REGISTER_RETRY_SEC 1
#define ADV_REGISTER_RETRY_MAX 30
/* SoftAP (tkl_wifi) may wait on this; also used as air-ready marker */
#define BLE_ADV_OK_FLAG "/tmp/ble_adv_ok"

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct adv_ctx {
    DBusConnection *conn;
    GDBusClient *client;
    GDBusProxy *manager;
    gboolean object_registered;
    gboolean want_enabled;
    gboolean registered;
    gboolean register_pending;
    guint retry_id;
    int retry_cnt;
    uint32_t min_interval_ms;
    uint32_t max_interval_ms;
    char *service_uuids[ADV_SVC_UUID_MAX];
    int service_uuid_cnt;
    char *service_data_uuid;
    uint8_t *service_data;
    int service_data_len;
    uint16_t mfg_id;
    uint8_t *mfg_data;
    int mfg_data_len;
    char *local_name;
    gboolean discoverable;
};

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static struct adv_ctx s_adv;

/* ---------------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------------- */
static void __try_register_adv(void);
static gboolean __register_adv_retry_cb(gpointer user_data);
static void __parse_ad_fields(const uint8_t *data, uint8_t len);

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Append one 16-bit service UUID as short hex ("fd50")
 * @param[in] uuid16 little-endian UUID from AD
 * @return none
 * @note Must stay 16-bit short form. Full 128-bit UUID makes BlueZ emit
 *       AD type 0x06/0x07/0x21 and can drop Tuya's required 0x16 Service Data.
 */
static void __add_service_uuid16(uint16_t uuid16)
{
    char *uuid = NULL;
    int i = 0;

    if (s_adv.service_uuid_cnt >= ADV_SVC_UUID_MAX) {
        return;
    }
    uuid = g_strdup_printf("%04x", uuid16);
    if (uuid == NULL) {
        return;
    }
    for (i = 0; i < s_adv.service_uuid_cnt; i++) {
        if ((s_adv.service_uuids[i] != NULL) && (g_ascii_strcasecmp(s_adv.service_uuids[i], uuid) == 0)) {
            g_free(uuid);
            return;
        }
    }
    s_adv.service_uuids[s_adv.service_uuid_cnt++] = uuid;
}

/**
 * @brief Parse HCI AD structures into BlueZ LEAdvertisement1 fields
 * @param[in] data AD payload
 * @param[in] len payload length
 * @return none
 * @note Flags (0x01) are ignored for Discoverable; always keep discoverable=TRUE
 *       so Tuya App scanners that filter LE General Discoverable still see us.
 */
static void __parse_ad_fields(const uint8_t *data, uint8_t len)
{
    uint8_t i = 0;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    while (i < len) {
        uint8_t field_len = data[i];
        uint8_t ad_type = 0;
        const uint8_t *payload = NULL;
        uint8_t plen = 0;

        if ((field_len == 0) || ((uint16_t)i + 1u + field_len > len)) {
            break;
        }
        ad_type = data[i + 1];
        payload = &data[i + 2];
        plen = (uint8_t)(field_len - 1);

        switch (ad_type) {
        case 0x01: /* Flags — do not clear Discoverable from limited-only flags */
            break;
        case 0x02: /* Incomplete 16-bit UUIDs */
        case 0x03: /* Complete 16-bit UUIDs */
            if ((plen >= 2) && ((plen % 2) == 0)) {
                uint8_t k = 0;
                for (k = 0; k + 1 < plen; k += 2) {
                    uint16_t u = (uint16_t)(payload[k] | (payload[k + 1] << 8));
                    __add_service_uuid16(u);
                }
            }
            break;
        case 0x16: /* Service Data - 16-bit UUID */
            if (plen >= 2) {
                uint16_t u = (uint16_t)(payload[0] | (payload[1] << 8));
                g_free(s_adv.service_data_uuid);
                g_free(s_adv.service_data);
                /*
                 * Short "fd50" only. BlueZ maps this to AD type 0x16.
                 * A 128-bit UUID string becomes type 0x21, which Tuya App
                 * scanners do not treat as the netcfg beacon.
                 */
                s_adv.service_data_uuid = g_strdup_printf("%04x", u);
                /* ServiceData already carries UUID; skip ServiceUUIDs to save ADV space */
                s_adv.service_data_len = plen - 2;
                if (s_adv.service_data_len > 0) {
                    s_adv.service_data = g_memdup(payload + 2, s_adv.service_data_len);
                } else {
                    s_adv.service_data = g_malloc0(1);
                    s_adv.service_data_len = 0;
                }
            }
            break;
        case 0xff: /* Manufacturer Specific Data */
            if (plen >= 2) {
                s_adv.mfg_id = (uint16_t)(payload[0] | (payload[1] << 8));
                g_free(s_adv.mfg_data);
                s_adv.mfg_data_len = plen - 2;
                if (s_adv.mfg_data_len > 0) {
                    s_adv.mfg_data = g_memdup(payload + 2, s_adv.mfg_data_len);
                } else {
                    s_adv.mfg_data = NULL;
                }
            }
            break;
        case 0x08: /* Shortened Local Name */
        case 0x09: /* Complete Local Name */
            if (plen > 0) {
                g_free(s_adv.local_name);
                s_adv.local_name = g_strndup((const char *)payload, plen);
            }
            break;
        default:
            break;
        }
        i = (uint8_t)(i + field_len + 1);
    }
}

/**
 * @brief Trim BlueZ LEAdvertisement fields to fit legacy ADV (31 bytes)
 * @return none
 * @note Legacy ADV max is 31 bytes. Historical scannable Tuya PDU is exactly:
 *       Flags(3) + ServiceUUID fd50(4) + ServiceData(24) = 31.
 *       BlueZ folds ManufacturerData AND LocalName into the ADV PDU. With both
 *       the FD50 ServiceData (24B) and the scan-rsp LocalName "TUYA_" (7B) the
 *       PDU becomes 38B and the controller rejects it:
 *         "Bluetooth: hci0: adv larger than maximum supported"
 *       BlueZ still reports ActiveInstances=1, but the adv never airs, so the
 *       phone never sees it. Drop ManufacturerData AND LocalName; Tuya netcfg
 *       matches on FD50 ServiceData, not the name.
 */
static void __trim_for_legacy_adv(void)
{
    if (s_adv.mfg_data != NULL || s_adv.mfg_id != 0) {
        PR_INFO("legacy ADV: drop ManufacturerData id=0x%04x len=%d (keep UUID+ServiceData)",
                s_adv.mfg_id, s_adv.mfg_data_len);
        g_free(s_adv.mfg_data);
        s_adv.mfg_data = NULL;
        s_adv.mfg_data_len = 0;
        s_adv.mfg_id = 0;
    }
    if (s_adv.local_name != NULL) {
        PR_INFO("legacy ADV: drop LocalName '%s' (PDU full at 31B without it)",
                s_adv.local_name);
        g_free(s_adv.local_name);
        s_adv.local_name = NULL;
    }
}

/**
 * @brief Get Type property
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_type(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    const char *type = "peripheral";

    (void)property;
    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &type);
    return TRUE;
}

/**
 * @brief Get ServiceUUIDs property
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_service_uuids(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    DBusMessageIter array;
    int i = 0;

    (void)property;
    (void)user_data;
    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "s", &array);
    for (i = 0; i < s_adv.service_uuid_cnt; i++) {
        dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &s_adv.service_uuids[i]);
    }
    dbus_message_iter_close_container(iter, &array);
    return TRUE;
}

/**
 * @brief Whether ServiceUUIDs property exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if any UUID cached
 */
static gboolean adv_exist_service_uuids(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return s_adv.service_uuid_cnt > 0;
}

/**
 * @brief Get ServiceData property (a{sv} with ay values)
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_service_data(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    DBusMessageIter dict;
    DBusMessageIter entry;
    DBusMessageIter variant;
    DBusMessageIter array;
    const uint8_t *value = NULL;

    (void)property;
    (void)user_data;
    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    if (s_adv.service_data_uuid != NULL) {
        value = (s_adv.service_data != NULL) ? s_adv.service_data : (const uint8_t *)"";
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &s_adv.service_data_uuid);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &array);
        dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &value, s_adv.service_data_len);
        dbus_message_iter_close_container(&variant, &array);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }
    dbus_message_iter_close_container(iter, &dict);
    return TRUE;
}

/**
 * @brief Whether ServiceData property exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if service data UUID cached (payload may be empty)
 */
static gboolean adv_exist_service_data(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return (s_adv.service_data_uuid != NULL);
}

/**
 * @brief Get ManufacturerData property (a{qv})
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_manufacturer_data(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    DBusMessageIter dict;
    DBusMessageIter entry;
    DBusMessageIter variant;
    DBusMessageIter array;
    const uint8_t *value = NULL;

    (void)property;
    (void)user_data;
    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{qv}", &dict);
    if ((s_adv.mfg_data != NULL) && (s_adv.mfg_data_len > 0)) {
        value = s_adv.mfg_data;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_UINT16, &s_adv.mfg_id);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &array);
        dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &value, s_adv.mfg_data_len);
        dbus_message_iter_close_container(&variant, &array);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }
    dbus_message_iter_close_container(iter, &dict);
    return TRUE;
}

/**
 * @brief Whether ManufacturerData property exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if manufacturer data cached
 */
static gboolean adv_exist_manufacturer_data(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return (s_adv.mfg_data != NULL) && (s_adv.mfg_data_len > 0);
}

/**
 * @brief Get LocalName property
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_local_name(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    (void)property;
    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &s_adv.local_name);
    return TRUE;
}

/**
 * @brief Whether LocalName property exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if name cached
 */
static gboolean adv_exist_local_name(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return (s_adv.local_name != NULL) && (s_adv.local_name[0] != '\0');
}

/**
 * @brief Get Discoverable property
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_discoverable(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    dbus_bool_t disc = TRUE;

    (void)property;
    (void)user_data;
    /* Always advertise as general-discoverable for Tuya netcfg scanners. */
    (void)s_adv.discoverable;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &disc);
    return TRUE;
}

/**
 * @brief Get MinInterval property (milliseconds)
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_min_interval(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    (void)property;
    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &s_adv.min_interval_ms);
    return TRUE;
}

/**
 * @brief Whether MinInterval exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if interval configured
 */
static gboolean adv_exist_min_interval(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return s_adv.min_interval_ms >= 20;
}

/**
 * @brief Get MaxInterval property (milliseconds)
 * @param[in] property unused
 * @param[out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return TRUE
 */
static gboolean adv_get_max_interval(const GDBusPropertyTable *property, DBusMessageIter *iter, void *user_data)
{
    (void)property;
    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &s_adv.max_interval_ms);
    return TRUE;
}

/**
 * @brief Whether MaxInterval exists
 * @param[in] property unused
 * @param[in] user_data unused
 * @return TRUE if interval configured
 */
static gboolean adv_exist_max_interval(const GDBusPropertyTable *property, void *user_data)
{
    (void)property;
    (void)user_data;
    return s_adv.max_interval_ms >= 20;
}

/**
 * @brief Release method from BlueZ when advertisement is removed
 * @param[in] conn unused
 * @param[in] msg method call
 * @param[in] user_data unused
 * @return method return message
 */
static DBusMessage *adv_release(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    (void)conn;
    (void)user_data;
    PR_WARN("LEAdvertisement Release()");
    s_adv.registered = FALSE;
    s_adv.register_pending = FALSE;
    return dbus_message_new_method_return(msg);
}

static const GDBusMethodTable adv_methods[] = {
    {GDBUS_NOREPLY_METHOD("Release", NULL, NULL, adv_release)},
    {}};

static const GDBusPropertyTable adv_properties[] = {
    {"Type", "s", adv_get_type},
    {"ServiceUUIDs", "as", adv_get_service_uuids, NULL, adv_exist_service_uuids},
    {"ServiceData", "a{sv}", adv_get_service_data, NULL, adv_exist_service_data},
    {"ManufacturerData", "a{qv}", adv_get_manufacturer_data, NULL, adv_exist_manufacturer_data},
    {"LocalName", "s", adv_get_local_name, NULL, adv_exist_local_name},
    {"Discoverable", "b", adv_get_discoverable},
    {"MinInterval", "u", adv_get_min_interval, NULL, adv_exist_min_interval},
    {"MaxInterval", "u", adv_get_max_interval, NULL, adv_exist_max_interval},
    {}};

/**
 * @brief Schedule RegisterAdvertisement retry
 * @return none
 */
static void __schedule_register_retry(void)
{
    if (s_adv.retry_id != 0) {
        return;
    }
    if (s_adv.retry_cnt >= ADV_REGISTER_RETRY_MAX) {
        PR_ERR("RegisterAdvertisement retry exhausted (%d)", s_adv.retry_cnt);
        return;
    }
    s_adv.retry_id = g_timeout_add_seconds(ADV_REGISTER_RETRY_SEC, __register_adv_retry_cb, NULL);
}

/**
 * @brief Dump ActiveInstances / Advertising for air confirmation
 * @return none
 */
static void __dump_adv_air_state(void)
{
    (void)system("dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 "
                 "org.freedesktop.DBus.Properties.Get "
                 "string:org.bluez.LEAdvertisingManager1 string:ActiveInstances "
                 "2>/dev/null | tee -a /var/log/ble_diag.log");
    (void)system("dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 "
                 "org.freedesktop.DBus.Properties.Get "
                 "string:org.bluez.Adapter1 string:Advertising "
                 "2>/dev/null | tee -a /var/log/ble_diag.log");
    (void)system("hciconfig hci0 2>/dev/null | grep -E 'TX bytes|RX bytes|BD Address' "
                 ">> /var/log/ble_diag.log");
}

/* ---------------------------------------------------------------------------
 * Periodic ADV health monitor -- diagnose "RegisterAdvertisement OK but phones
 * never see the beacon". Dumps the advertisement object state once, then logs
 * BlueZ/controller/wifi state every few seconds so we can tell whether ADV
 * flaps (registered toggles) or whether wlan1/hostapd activity correlates.
 * --------------------------------------------------------------------------- */
#define ADV_HEALTH_INTERVAL_SEC 5
#define ADV_HEALTH_MAX_TICKS    60   /* 5 minutes */
static guint g_adv_health_id = 0;
static int g_adv_health_tick = 0;

static void __dump_adv_object_state(void)
{
    int i = 0;
    char hex[ADV_DATA_MAX * 3 + 4];
    unsigned int n = 0;

    PR_INFO("==== ADV object state (what BlueZ reads) ====");
    PR_INFO("Type=peripheral Discoverable=true Interval=%u-%ums",
            s_adv.min_interval_ms, s_adv.max_interval_ms);
    PR_INFO("ServiceUUIDs(cnt=%d):", s_adv.service_uuid_cnt);
    for (i = 0; i < s_adv.service_uuid_cnt; i++) {
        PR_INFO("  uuid[%d]=%s", i, s_adv.service_uuids[i] ? s_adv.service_uuids[i] : "(null)");
    }
    PR_INFO("ServiceData uuid=%s len=%d",
            s_adv.service_data_uuid ? s_adv.service_data_uuid : "(none)", s_adv.service_data_len);
    hex[0] = '\0';
    if (s_adv.service_data && s_adv.service_data_len > 0) {
        int j = 0;
        for (j = 0; (j < s_adv.service_data_len) && (n + 3 < sizeof(hex)); j++) {
            int w = snprintf(hex + n, sizeof(hex) - n, "%02x ", s_adv.service_data[j]);
            if (w <= 0) {
                break;
            }
            n += (unsigned int)w;
        }
    }
    PR_INFO("ServiceData bytes: %s", hex);
    PR_INFO("ManufacturerData id=0x%04x len=%d  LocalName=%s",
            s_adv.mfg_id, s_adv.mfg_data_len, s_adv.local_name ? s_adv.local_name : "(none, dropped)");
    PR_INFO("registered=%d want_enabled=%d object_registered=%d",
            s_adv.registered, s_adv.want_enabled, s_adv.object_registered);
    PR_INFO("==== ADV object state end ====");
}

static gboolean __adv_health_cb(gpointer user_data)
{
    (void)user_data;
    g_adv_health_tick++;
    PR_INFO("[adv-health #%d] registered=%d want=%d",
            g_adv_health_tick, s_adv.registered, s_adv.want_enabled);
    (void)system("echo '---- adv-health ----' >> /var/log/ble_diag.log");
    (void)system("dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 "
                 "org.freedesktop.DBus.Properties.Get "
                 "string:org.bluez.LEAdvertisingManager1 string:ActiveInstances "
                 ">> /var/log/ble_diag.log 2>&1");
    (void)system("hciconfig hci0 >> /var/log/ble_diag.log 2>&1");
    (void)system("{ echo -n 'wlan1='; cat /sys/class/net/wlan1/operstate 2>/dev/null; "
                 "echo -n ' hostapd='; pidof hostapd >/dev/null 2>&1 && echo up || echo down; } "
                 ">> /var/log/ble_diag.log");
    if (g_adv_health_tick >= ADV_HEALTH_MAX_TICKS) {
        PR_INFO("[adv-health] monitor finished after %d ticks", g_adv_health_tick);
        g_adv_health_id = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

/**
 * @brief Handle RegisterAdvertisement reply
 * @param[in] reply D-Bus reply
 * @param[in] user_data unused
 * @return none
 */
static void register_adv_reply(DBusMessage *reply, void *user_data)
{
    DBusError derr;

    (void)user_data;
    s_adv.register_pending = FALSE;
    dbus_error_init(&derr);
    dbus_set_error_from_message(&derr, reply);

    if (dbus_error_is_set(&derr)) {
        s_adv.registered = FALSE;
        PR_ERR("RegisterAdvertisement: %s", derr.message);
        if (s_adv.want_enabled) {
            __schedule_register_retry();
        }
    } else {
        int fd = -1;

        s_adv.registered = TRUE;
        s_adv.retry_cnt = 0;
        if (s_adv.retry_id != 0) {
            g_source_remove(s_adv.retry_id);
            s_adv.retry_id = 0;
        }
        PR_INFO("RegisterAdvertisement: OK");
        fd = open(BLE_ADV_OK_FLAG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            (void)write(fd, "1\n", 2);
            close(fd);
        }
        __dump_adv_air_state();
        __dump_adv_object_state();
        if (g_adv_health_id == 0) {
            g_adv_health_tick = 0;
            g_adv_health_id = g_timeout_add_seconds(ADV_HEALTH_INTERVAL_SEC, __adv_health_cb, NULL);
            PR_INFO("[adv-health] monitor started (every %ds, %d ticks)",
                    ADV_HEALTH_INTERVAL_SEC, ADV_HEALTH_MAX_TICKS);
        }
        /*
         * Keep SoftAP running. nc_tp SoftAP+BLE needs hostapd while App is
         * connected over BLE; pausing SoftAP after ADV made discovery work
         * but broke provisioning (SSID gone / AP TCP fails).
         */
        PR_INFO("SoftAP kept up for dual SoftAP+BLE netcfg");
    }
    dbus_error_free(&derr);
}

/**
 * @brief Fill RegisterAdvertisement arguments
 * @param[in,out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return none
 */
static void register_adv_setup(DBusMessageIter *iter, void *user_data)
{
    const char *path = ADV_PATH;
    DBusMessageIter dict;

    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
    dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(iter, &dict);
}

/**
 * @brief Issue RegisterAdvertisement when manager and data are ready
 * @return none
 */
static void __try_register_adv(void)
{
    if (!s_adv.want_enabled || s_adv.registered || s_adv.register_pending) {
        return;
    }
    if (!s_adv.object_registered) {
        PR_DEBUG("RegisterAdvertisement deferred: object not ready");
        return;
    }
    if (s_adv.manager == NULL) {
        PR_DEBUG("RegisterAdvertisement deferred: manager not ready");
        return;
    }
    if ((s_adv.service_uuid_cnt == 0) && (s_adv.service_data_uuid == NULL) && (s_adv.mfg_data == NULL) &&
        (s_adv.local_name == NULL)) {
        PR_DEBUG("RegisterAdvertisement deferred: no adv data");
        return;
    }

    if (!g_dbus_proxy_method_call(s_adv.manager, "RegisterAdvertisement", register_adv_setup, register_adv_reply, NULL,
                                  NULL)) {
        PR_ERR("Unable to call RegisterAdvertisement");
        __schedule_register_retry();
        return;
    }

    s_adv.register_pending = TRUE;
    PR_INFO("RegisterAdvertisement requested");
}

/**
 * @brief GLib timeout to retry RegisterAdvertisement
 * @param[in] user_data unused
 * @return G_SOURCE_REMOVE
 */
static gboolean __register_adv_retry_cb(gpointer user_data)
{
    (void)user_data;
    s_adv.retry_id = 0;
    s_adv.retry_cnt++;
    PR_WARN("RegisterAdvertisement retry %d/%d", s_adv.retry_cnt, ADV_REGISTER_RETRY_MAX);
    __try_register_adv();
    return G_SOURCE_REMOVE;
}

/**
 * @brief Handle UnregisterAdvertisement reply
 * @param[in] reply D-Bus reply
 * @param[in] user_data unused
 * @return none
 */
static void unregister_adv_reply(DBusMessage *reply, void *user_data)
{
    DBusError derr;

    (void)user_data;
    dbus_error_init(&derr);
    dbus_set_error_from_message(&derr, reply);
    if (dbus_error_is_set(&derr)) {
        PR_WARN("UnregisterAdvertisement: %s", derr.message);
    } else {
        PR_INFO("UnregisterAdvertisement: OK");
    }
    s_adv.registered = FALSE;
    s_adv.register_pending = FALSE;
    dbus_error_free(&derr);
}

/**
 * @brief Fill UnregisterAdvertisement arguments
 * @param[in,out] iter D-Bus iterator
 * @param[in] user_data unused
 * @return none
 */
static void unregister_adv_setup(DBusMessageIter *iter, void *user_data)
{
    const char *path = ADV_PATH;

    (void)user_data;
    dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);
}

/**
 * @brief Unregister current advertisement if registered
 * @return none
 */
static void __unregister_adv(void)
{
    if ((s_adv.manager == NULL) || (!s_adv.registered && !s_adv.register_pending)) {
        s_adv.registered = FALSE;
        s_adv.register_pending = FALSE;
        return;
    }

    if (!g_dbus_proxy_method_call(s_adv.manager, "UnregisterAdvertisement", unregister_adv_setup, unregister_adv_reply,
                                  NULL, NULL)) {
        PR_WARN("Unable to call UnregisterAdvertisement");
        s_adv.registered = FALSE;
        s_adv.register_pending = FALSE;
        return;
    }
}

/**
 * @brief Cache LEAdvertisingManager1 proxy
 * @param[in] proxy new proxy
 * @param[in] user_data unused
 * @return none
 */
static void adv_proxy_added_cb(GDBusProxy *proxy, void *user_data)
{
    const char *iface = NULL;

    (void)user_data;
    iface = g_dbus_proxy_get_interface(proxy);
    if (g_strcmp0(iface, LE_ADV_MGR_IFACE) != 0) {
        return;
    }

    s_adv.manager = proxy;
    PR_INFO("LEAdvertisingManager1 ready");
    __try_register_adv();
}

/**
 * @brief Clear LEAdvertisingManager1 proxy on removal
 * @param[in] proxy removed proxy
 * @param[in] user_data unused
 * @return none
 */
static void adv_proxy_removed_cb(GDBusProxy *proxy, void *user_data)
{
    (void)user_data;
    if (proxy != s_adv.manager) {
        return;
    }
    PR_WARN("LEAdvertisingManager1 removed");
    s_adv.manager = NULL;
    s_adv.registered = FALSE;
    s_adv.register_pending = FALSE;
}

/**
 * @brief Initialize LE advertisement D-Bus object on shared connection
 * @param[in] conn system bus connection (from tuya_gatt_init)
 * @return LE_SUCCESS on success, error code on failure
 */
int tuya_adv_init(DBusConnection *conn)
{
    if (conn == NULL) {
        return LE_INVALID_PARAM;
    }
    if (s_adv.conn != NULL) {
        return LE_SUCCESS;
    }

    memset(&s_adv, 0, sizeof(s_adv));
    s_adv.conn = conn;
    s_adv.discoverable = TRUE;
    s_adv.min_interval_ms = 100;
    s_adv.max_interval_ms = 120;

    if (!g_dbus_register_interface(s_adv.conn, ADV_PATH, LE_ADV_IFACE, adv_methods, NULL, adv_properties, NULL, NULL)) {
        PR_ERR("Couldn't register LEAdvertisement1 at %s", ADV_PATH);
        s_adv.conn = NULL;
        return LE_COM_ERROR;
    }
    s_adv.object_registered = TRUE;

    s_adv.client = g_dbus_client_new(s_adv.conn, "org.bluez", "/");
    g_dbus_client_set_proxy_handlers(s_adv.client, adv_proxy_added_cb, adv_proxy_removed_cb, NULL, NULL);

    PR_INFO("LEAdvertisement1 exported at %s", ADV_PATH);
    return LE_SUCCESS;
}

/**
 * @brief Cache HCI-slot advertising parameters (interval in 0.625ms units)
 * @param[in] min_interval minimum advertising interval
 * @param[in] max_interval maximum advertising interval
 * @param[in] advtype unused (BlueZ Type is peripheral)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_params(uint16_t min_interval, uint16_t max_interval, uint8_t advtype)
{
    uint32_t min_ms = 0;
    uint32_t max_ms = 0;

    (void)advtype;
    /* HCI units are 0.625 ms; BlueZ uses milliseconds. */
    min_ms = (uint32_t)(((uint32_t)min_interval * 625u) + 999u) / 1000u;
    max_ms = (uint32_t)(((uint32_t)max_interval * 625u) + 999u) / 1000u;
    if (min_ms < 20) {
        min_ms = 20;
    }
    /*
     * Force fast advertising (~10-20 Hz) for netcfg discoverability. RTL8733BU
     * is a WiFi+BT combo chip; with the wlan1 SoftAP up, BT coexist gives BLE
     * only intermittent RF windows. A slower interval lets the phone miss the
     * beacon between windows, so cap to a fast discovery range.
     */
    if (min_ms > 48) {
        min_ms = 48;
    }
    if (max_ms > 100) {
        max_ms = 100;
    }
    if (max_ms < min_ms) {
        max_ms = min_ms;
    }
    s_adv.min_interval_ms = min_ms;
    s_adv.max_interval_ms = max_ms;
    PR_INFO("BlueZ adv interval %u-%u ms", s_adv.min_interval_ms, s_adv.max_interval_ms);
    return LE_SUCCESS;
}

/**
 * @brief Cache and parse advertising payload (HCI AD format)
 * @param[in] data advertising payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_data(uint8_t *data, uint8_t len)
{
    char hex[ADV_DATA_MAX * 3 + 4];
    unsigned int n = 0;
    uint8_t j = 0;

    if ((data == NULL) || (len == 0) || (len > ADV_DATA_MAX)) {
        return LE_INVALID_PARAM;
    }

    hex[0] = '\0';
    for (j = 0; (j < len) && (n + 3 < sizeof(hex)); j++) {
        int w = snprintf(hex + n, sizeof(hex) - n, "%02x ", data[j]);
        if (w <= 0) {
            break;
        }
        n += (unsigned int)w;
    }

    /* Keep scan-rsp derived fields; clear only fields typically in ADV PDU. */
    {
        int i = 0;
        for (i = 0; i < s_adv.service_uuid_cnt; i++) {
            g_free(s_adv.service_uuids[i]);
            s_adv.service_uuids[i] = NULL;
        }
        s_adv.service_uuid_cnt = 0;
        g_free(s_adv.service_data_uuid);
        s_adv.service_data_uuid = NULL;
        g_free(s_adv.service_data);
        s_adv.service_data = NULL;
        s_adv.service_data_len = 0;
    }
    __parse_ad_fields(data, len);
    __trim_for_legacy_adv();
    PR_INFO("BlueZ adv raw[%u]: %s", (unsigned)len, hex);
    PR_INFO("BlueZ adv parsed uuids=%d svc_uuid=%s svc_data_len=%d", s_adv.service_uuid_cnt,
            s_adv.service_data_uuid ? s_adv.service_data_uuid : "(none)", s_adv.service_data_len);

    if (s_adv.want_enabled && s_adv.registered) {
        __unregister_adv();
        /* Re-register after unregister completes asynchronously; schedule retry. */
        s_adv.registered = FALSE;
        __schedule_register_retry();
    } else if (s_adv.want_enabled) {
        __try_register_adv();
    }
    return LE_SUCCESS;
}

/**
 * @brief Cache and parse scan response payload (HCI AD format)
 * @param[in] data scan response payload
 * @param[in] len payload length (1..31)
 * @return LE_SUCCESS on success
 */
int tuya_adv_set_scan_rsp(uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len == 0) || (len > ADV_DATA_MAX)) {
        return LE_INVALID_PARAM;
    }

    g_free(s_adv.mfg_data);
    s_adv.mfg_data = NULL;
    s_adv.mfg_data_len = 0;
    s_adv.mfg_id = 0;
    g_free(s_adv.local_name);
    s_adv.local_name = NULL;

    __parse_ad_fields(data, len);
    /* ManufacturerData must not land in ADV PDU under legacy 31B limit. */
    __trim_for_legacy_adv();
    PR_INFO("BlueZ scan rsp parsed mfg=%d name=%s", s_adv.mfg_data_len,
            s_adv.local_name ? s_adv.local_name : "(none)");

    if (s_adv.want_enabled && s_adv.registered) {
        __unregister_adv();
        s_adv.registered = FALSE;
        __schedule_register_retry();
    } else if (s_adv.want_enabled) {
        __try_register_adv();
    }
    return LE_SUCCESS;
}

/**
 * @brief Enable or disable advertising via RegisterAdvertisement
 * @param[in] enable true to enable, false to disable
 * @return LE_SUCCESS if request accepted (may complete asynchronously)
 */
int tuya_adv_set_enable(bool enable)
{
    s_adv.want_enabled = enable ? TRUE : FALSE;

    if (!enable) {
        if (s_adv.retry_id != 0) {
            g_source_remove(s_adv.retry_id);
            s_adv.retry_id = 0;
        }
        s_adv.retry_cnt = 0;
        __unregister_adv();
        PR_INFO("BlueZ adv disable requested");
        return LE_SUCCESS;
    }

    if (s_adv.conn == NULL) {
        return LE_COM_ERROR;
    }

    s_adv.retry_cnt = 0;
    __try_register_adv();
    if ((s_adv.manager == NULL) || (!s_adv.registered && !s_adv.register_pending)) {
        __schedule_register_retry();
    }
    PR_INFO("BlueZ adv enable requested");
    return LE_SUCCESS;
}

/**
 * @brief Whether LEAdvertisingManager1 proxy is ready
 * @return true if manager proxy is available
 */
bool tuya_adv_manager_ready(void)
{
    return s_adv.manager != NULL;
}
