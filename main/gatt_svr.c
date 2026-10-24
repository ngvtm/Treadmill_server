#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"
#include "services/ans/ble_svc_ans.h"
#include "treadmill/treadmill.h"
#include "ir_safety/ir_safety.h"

#define MAX_NOTIFY 5
#define TREADMILL_MSG_STATUS 0x80
#define SAFETY_MONITOR_STACK_SIZE 12288
// Health-probe period while treadmill is OFF. 200ms for realtime bench-testing
// of mount height (watch raw distances react as you move a sensor); raise back
// toward 2000ms for normal use.
#define IR_HEALTH_PERIOD_MS 200

typedef enum {
    TREADMILL_ALERT_NONE = 0,
    TREADMILL_ALERT_WARNING_1,
    TREADMILL_ALERT_WARNING_2,
    TREADMILL_ALERT_DANGER,
    TREADMILL_ALERT_NO_PERSON,
    TREADMILL_ALERT_CMD_REJECTED,
    TREADMILL_ALERT_SAFETY_KEY_OUT,  /* reed breakaway key pulled / not docked (=6) */
} treadmill_alert_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t running;
    uint8_t speed;
    uint8_t requested_speed;
    uint8_t safety_limited;
    uint8_t zone;
    uint8_t alert;
} treadmill_status_msg_t;

static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

static uint8_t  gatt_svr_chr_val;
static uint16_t gatt_svr_chr_val_handle;
static uint16_t gatt_svr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static treadmill_status_msg_t gatt_svr_status_val = {
    .type = TREADMILL_MSG_STATUS,
};
static const ble_uuid128_t gatt_svr_chr_uuid =
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
                     0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33);

static uint8_t gatt_svr_dsc_val;
static const ble_uuid128_t gatt_svr_dsc_uuid =
    BLE_UUID128_INIT(0x01, 0x01, 0x01, 0x01, 0x12, 0x12, 0x12, 0x12,
                     0x23, 0x23, 0x23, 0x23, 0x34, 0x34, 0x34, 0x34);

static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static void gatt_svr_update_status(uint8_t alert, uint8_t zone);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
            .uuid        = &gatt_svr_chr_uuid.u,
            .access_cb   = gatt_svc_access,
            /* FIX: bỏ _ENC flags vì không dùng encryption nữa */
            .flags       = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                           BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
            .val_handle  = &gatt_svr_chr_val_handle,
            .descriptors = (struct ble_gatt_dsc_def[])
            { {
                .uuid      = &gatt_svr_dsc_uuid.u,
                .att_flags = BLE_ATT_F_READ,
                .access_cb = gatt_svc_access,
            }, {
                0,
            } },
        }, {
            0,
        } },
    },
    {
        0,
    },
};

static int
gatt_svr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
               void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int
gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    switch (ctxt->op) {

    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            MODLOG_DFLT(INFO, "Characteristic read; conn_handle=%d attr_handle=%d\n",
                        conn_handle, attr_handle);
        } else {
            MODLOG_DFLT(INFO, "Characteristic read by NimBLE stack; attr_handle=%d\n",
                        attr_handle);
        }
        uuid = ctxt->chr->uuid;
        if (attr_handle == gatt_svr_chr_val_handle) {
            gatt_svr_update_status(TREADMILL_ALERT_NONE,
                                   ir_safety_get_cached_zone());
            rc = os_mbuf_append(ctxt->om,
                                &gatt_svr_status_val,
                                sizeof(gatt_svr_status_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            MODLOG_DFLT(INFO, "Characteristic write; conn_handle=%d attr_handle=%d",
                        conn_handle, attr_handle);
        } else {
            MODLOG_DFLT(INFO, "Characteristic write by NimBLE stack; attr_handle=%d",
                        attr_handle);
        }
        uuid = ctxt->chr->uuid;
        if (attr_handle == gatt_svr_chr_val_handle) {
            rc = gatt_svr_write(ctxt->om,
                                sizeof(gatt_svr_chr_val),
                                sizeof(gatt_svr_chr_val),
                                &gatt_svr_chr_val, NULL);
            if (rc == 0) {
                treadmill_handle_command(gatt_svr_chr_val);
                ir_zone_t zone = ir_safety_get_cached_zone();
                if (gatt_svr_chr_val == TREADMILL_CMD_ON &&
                    !treadmill_is_running()) {
                    /* Name the real blocker: a pulled/missing safety
                     * key is a different fix than a VL53 rejection. */
                    uint8_t alert =
                        (treadmill_is_safety_key_tripped() ||
                         !treadmill_is_safety_key_present())
                            ? TREADMILL_ALERT_SAFETY_KEY_OUT
                            : TREADMILL_ALERT_CMD_REJECTED;
                    gatt_svr_notify_status(alert, zone);
                } else {
                    gatt_svr_notify_status(TREADMILL_ALERT_NONE,
                                           zone);
                }
            }
            return rc;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_READ_DSC:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            MODLOG_DFLT(INFO, "Descriptor read; conn_handle=%d attr_handle=%d\n",
                        conn_handle, attr_handle);
        } else {
            MODLOG_DFLT(INFO, "Descriptor read by NimBLE stack; attr_handle=%d\n",
                        attr_handle);
        }
        uuid = ctxt->dsc->uuid;
        if (ble_uuid_cmp(uuid, &gatt_svr_dsc_uuid.u) == 0) {
            rc = os_mbuf_append(ctxt->om,
                                &gatt_svr_dsc_val,
                                sizeof(gatt_svr_dsc_val)); /* FIX: đúng variable */
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        goto unknown;

    default:
        goto unknown;
    }

unknown:
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_set_connection(uint16_t conn_handle)
{
    gatt_svr_conn_handle = conn_handle;
}

void gatt_svr_clear_connection(uint16_t conn_handle)
{
    if (gatt_svr_conn_handle == conn_handle) {
        gatt_svr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
}

static void gatt_svr_update_status(uint8_t alert, uint8_t zone)
{
    gatt_svr_status_val.type = TREADMILL_MSG_STATUS;
    gatt_svr_status_val.running = treadmill_is_running() ? 1 : 0;
    gatt_svr_status_val.speed = treadmill_get_speed();
    gatt_svr_status_val.requested_speed = treadmill_get_requested_speed();
    gatt_svr_status_val.safety_limited = treadmill_is_safety_limited() ? 1 : 0;
    gatt_svr_status_val.zone = zone;
    gatt_svr_status_val.alert = alert;
}

void gatt_svr_notify_status(uint8_t alert, uint8_t zone)
{
    gatt_svr_update_status(alert, zone);
    if (gatt_svr_conn_handle == BLE_HS_CONN_HANDLE_NONE ||
        gatt_svr_chr_val_handle == 0) {
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&gatt_svr_status_val,
                                               sizeof(gatt_svr_status_val));
    if (om == NULL) {
        MODLOG_DFLT(ERROR, "status notify mbuf alloc failed\n");
        return;
    }

    int rc = ble_gatts_notify_custom(gatt_svr_conn_handle,
                                     gatt_svr_chr_val_handle,
                                     om);
    if (rc != 0) {
        MODLOG_DFLT(WARN, "status notify failed rc=%d\n", rc);
    }
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;
    default:
        assert(0);
        break;
    }
}

static uint8_t alert_for_zone(ir_zone_t zone)
{
    switch (zone) {
    case IR_ZONE_WARNING_1: return TREADMILL_ALERT_WARNING_1;
    case IR_ZONE_WARNING_2: return TREADMILL_ALERT_WARNING_2;
    case IR_ZONE_DANGER:    return TREADMILL_ALERT_DANGER;
    case IR_ZONE_NONE:      return TREADMILL_ALERT_NO_PERSON;
    case IR_ZONE_SAFE:
    default:                return TREADMILL_ALERT_NONE;
    }
}

static void safety_monitor_task(void *arg)
{
    ir_zone_t last_zone = IR_ZONE_NONE;
    bool have_last_zone = false;
    bool last_key_tripped = false;
    TickType_t last_health_tick = 0;

    while (1) {
        /* Reed safety key trip: the motor is already hard-cut by
         * the trip callback; here we only owe the smartkey the alert
         * the moment the latch flips. */
        bool key_tripped = treadmill_is_safety_key_tripped();
        if (key_tripped && !last_key_tripped) {
            gatt_svr_notify_status(TREADMILL_ALERT_SAFETY_KEY_OUT,
                                   ir_safety_get_cached_zone());
        }
        last_key_tripped = key_tripped;

        bool running = treadmill_is_running();
        ir_zone_t zone;

        if (running) {
            zone = ir_safety_get_zone();
        } else {
            // Keep watching while the belt is off so the smartkey still gets
            // told about DANGER; refresh on the health cadence, not every loop.
            if (xTaskGetTickCount() - last_health_tick < pdMS_TO_TICKS(IR_HEALTH_PERIOD_MS)) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            ir_safety_health_check();
            last_health_tick = xTaskGetTickCount();
            zone = ir_safety_get_cached_zone();
        }

        if (!have_last_zone || zone != last_zone) {
            have_last_zone = true;
            last_zone = zone;

            // Motor action only applies while running; the handler itself is a
            // no-op when stopped, but the alert goes out either way.
            if (running) {
                treadmill_handle_safety_zone(zone);
            }
            gatt_svr_notify_status(alert_for_zone(zone), zone);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    gatt_svr_dsc_val = 0x99;

    xTaskCreate(safety_monitor_task, "safety_mon",
                SAFETY_MONITOR_STACK_SIZE, NULL, 12, NULL);
    ESP_LOGI("GATT", "Safety monitor started");

    return 0;
}
