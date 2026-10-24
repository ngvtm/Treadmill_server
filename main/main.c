#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_idf_version.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "motor_feedback/motor_feedback.h"
#include "settings/settings.h"
#include "blackbox/blackbox.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"
#include "host/ble_sm.h"
#include "ir_safety/ir_safety.h"
#include "safety_key/safety_key.h"
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

    /* Đã có smartkey được cặp rồi thì chỉ cho đúng key đó kết nối:
     * nạp danh sách peer đã bond vào whitelist và chỉ nhận kết nối
     * từ whitelist. Chưa có bond nào (lần đầu / sau khi unpair) thì
     * mở cho mọi kết nối để thực hiện pairing lần đầu. */
    {
        ble_addr_t wl_addrs[8];
        int wl_num = 0;
        int wl_rc = ble_store_util_bonded_peers(wl_addrs, &wl_num, 8);
        if (wl_rc == 0 && wl_num > 0) {
            if (ble_gap_wl_set(wl_addrs, wl_num) == 0) {
                adv_params.filter_policy =
                    BLE_GAP_ADV_FILTER_POLICY_SCAN_ANY_CONN_WL;
                MODLOG_DFLT(INFO, "advertise: whitelist active, %d bonded peer(s)\n",
                            wl_num);
            }
        }
    }

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
            /* Yêu cầu mã hoá/bonding ngay sau khi kết nối. Lần đầu:
             * hai bên pairing và LTK được lưu vào store. Lần sau:
             * liên kết được mã hoá lại bằng LTK đã lưu, không pairing
             * lại. Kết quả về ở BLE_GAP_EVENT_ENC_CHANGE. */
            rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "security_initiate failed; rc=%d\n", rc);
            }
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

    case BLE_GAP_EVENT_ENC_CHANGE:
        MODLOG_DFLT(INFO, "encryption change; status=%d\n",
                    event->enc_change.status);
        if (event->enc_change.status != 0) {
            /* Không mã hoá được = không tin cậy: ngắt. Lệnh điều khiển
             * chỉ đi qua characteristic yêu cầu encryption (xem
             * gatt_svr.c), nhưng ngắt hẳn tại đây cho chắc. */
            ble_gap_terminate(event->enc_change.conn_handle,
                              BLE_ERR_REM_USER_CONN_TERM);
        }
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

    /* ★ KHÔNG được xoá store ở đây: ble_store_clear() chạy mỗi lần boot sẽ
     * xoá toàn bộ LTK/bond đã lưu trong NVS, khiến pairing không bao giờ
     * sống qua một lần khởi động lại. Đây là gốc rễ của lỗi "không lưu
     * được key". Bond chỉ bị xoá khi unpair chủ động (xem README/BLE). */

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

    // Phase 1, item 1.3: task watchdog over the safety paths. A hung
    // safety-monitor or safety-key task resets the chip (panic) instead
    // of leaving the last PWM running forever. Default 5 s, not 2 s:
    // sequential VL53 scans still take ~1.6 s, so a 2 s timeout would
    // false-trip on slow sweeps; tighten to 2 s once parallel ranging
    // (item 1.2) drops the monitor loop to ~100 ms. This is a software
    // net only — the GĐ2 hardware ENA pull-down / heartbeat interlock
    // is what guarantees the motor stops even across a reset.
#ifndef CONFIG_TREADMILL_TASK_WDT_TIMEOUT_S
#define CONFIG_TREADMILL_TASK_WDT_TIMEOUT_S 5
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {
        esp_task_wdt_config_t wdt_cfg = {
            .timeout_ms = CONFIG_TREADMILL_TASK_WDT_TIMEOUT_S * 1000,
            .idle_core_mask = 0,
            .trigger_panic = true,
        };
        ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_cfg));
    }
#else
    ESP_ERROR_CHECK(esp_task_wdt_init(CONFIG_TREADMILL_TASK_WDT_TIMEOUT_S, true));
#endif
    ESP_LOGI(tag, "task watchdog armed: %d s (safety_mon, safety_key)",
             CONFIG_TREADMILL_TASK_WDT_TIMEOUT_S);

    // GĐ4: black box first — before any task that can record events.
    blackbox_setup();
    blackbox_dump();
    ir_safety_init();
    safety_key_init();
    settings_init();
    treadmill_init();
    motor_feedback_init();

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    ble_hs_cfg.reset_cb          = bleprph_on_reset;
    ble_hs_cfg.sync_cb           = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    /* Bảo mật: bật bonding + LE Secure Connections, phân phối cả khoá
     * mã hoá (ENC) lẫn khoá định danh (ID) thì LTK/địa chỉ peer mới được
     * ghi vào store và dùng lại ở lần kết nối sau. sm_mitm = 0 (Just
     * Works) vì cả 2 thiết bị đều không có bàn phím; muốn chống nghe
     * trộm chủ động (MITM) thì nâng cấp passkey ở phase sau. */
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_io_cap         = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

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
