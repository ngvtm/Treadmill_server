#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"
#include "ir_safety/ir_safety.h"
#include "treadmill/treadmill.h"


static const char *tag = "NimBLE_BLE_PRPH";
static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;

void ble_store_config_init(void);

#if NIMBLE_BLE_CONNECT
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout);
}
#endif

static void
bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields  fields;
    int rc;

    memset(&fields, 0, sizeof fields);
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    const char *name        = ble_svc_gap_device_name();
    fields.name             = (uint8_t *)name;
    fields.name_len         = strlen(name);
    fields.name_is_complete = 1;
#endif

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16         = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
    }
}

static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
#if NIMBLE_BLE_CONNECT
    struct ble_gap_conn_desc desc;
    int rc;
#endif

    switch (event->type) {

#if NIMBLE_BLE_CONNECT
    case BLE_GAP_EVENT_CONNECT:
        MODLOG_DFLT(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
            gatt_svr_set_connection(event->connect.conn_handle);
            /* Không gọi security_initiate — không dùng encryption */
        } else {
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n",
                    event->disconnect.reason);
        treadmill_handle_command(TREADMILL_CMD_OFF);
        gatt_svr_clear_connection(event->disconnect.conn.conn_handle);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        MODLOG_DFLT(INFO, "connection updated; status=%d\n",
                    event->conn_update.status);
        return 0;
#endif

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d\n",
                    event->adv_complete.reason);
        bleprph_advertise();
        return 0;

#if NIMBLE_BLE_CONNECT
    case BLE_GAP_EVENT_NOTIFY_TX:
        MODLOG_DFLT(INFO, "notify_tx event; conn_handle=%d attr_handle=%d "
                          "status=%d is_indication=%d\n",
                    event->notify_tx.conn_handle,
                    event->notify_tx.attr_handle,
                    event->notify_tx.status,
                    event->notify_tx.indication);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle);
        if (event->subscribe.cur_notify) {
            gatt_svr_notify_status(0, ir_safety_get_cached_zone());
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
#endif
    }
    return 0;
}

static void
bleprph_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* ★ Gọi SAU khi host đã sẵn sàng */
    ble_store_clear();

    uint8_t addr_val[6] = {0};
    ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");

    bleprph_advertise();
}


void bleprph_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void
app_main(void)
{
    int rc;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ir_safety_init();
    treadmill_init();

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    ble_hs_cfg.reset_cb          = bleprph_on_reset;
    ble_hs_cfg.sync_cb           = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    /* ★ FIX: Tắt hoàn toàn bonding/security */
    ble_hs_cfg.sm_bonding        = 0;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 0;
    ble_hs_cfg.sm_io_cap         = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_our_key_dist   = 0;
    ble_hs_cfg.sm_their_key_dist = 0;

#if MYNEWT_VAL(BLE_GATTS)
    rc = gatt_svr_init();
    assert(rc == 0);
#endif

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    rc = ble_svc_gap_device_name_set("treadmill-server");
    assert(rc == 0);
#endif

    ble_store_config_init();
    nimble_port_freertos_init(bleprph_host_task);

    rc = scli_init();
    if (rc != ESP_OK) {
        ESP_LOGE(tag, "scli_init() failed");
    }
}
