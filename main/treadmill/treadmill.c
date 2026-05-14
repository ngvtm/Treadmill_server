#include "treadmill.h"
#include "motor/motor.h"
#include "ir_safety/ir_safety.h"   /* ★ THÊM */
#include "esp_log.h"

#define TAG "TREADMILL"

static bool s_running = false;
static uint8_t s_speed = TREADMILL_SPEED_DEFAULT;

void treadmill_init(void)
{
    s_running = false;
    s_speed = TREADMILL_SPEED_DEFAULT;
    motor_init();
    ESP_LOGI(TAG, "Initialized (state=OFF, speed=%d)", s_speed);
}

void treadmill_handle_command(uint8_t cmd)
{
    switch (cmd) {

    case TREADMILL_CMD_ON: {
        /* ★ SAFETY: phải có người ở vùng an toàn mới cho bật */
        ir_zone_t zone = ir_safety_get_zone();
        if (zone != IR_ZONE_SAFE) {
            ESP_LOGW(TAG, "SAFETY BLOCK: Từ chối BẬT — zone=%s",
                     zone == IR_ZONE_NONE ? "NONE (không có người)" 
                                          : "DANGER (vùng nguy hiểm)");
            return;  /* Bỏ qua lệnh, motor không chạy */
        }
        if (s_speed == 0) s_speed = TREADMILL_SPEED_DEFAULT;
        s_running = true;
        motor_set_direction(MOTOR_DIR_FORWARD);
        motor_set_speed(s_speed);
        ESP_LOGI(TAG, ">>> BẬT MÁY CHẠY | Speed: %d/%d", s_speed, TREADMILL_SPEED_MAX);
        break;
    }

    case TREADMILL_CMD_OFF:
        s_running = false;
        s_speed = TREADMILL_SPEED_DEFAULT;
        motor_stop();
        ESP_LOGI(TAG, ">>> TẮT MÁY CHẠY");
        break;

    case TREADMILL_CMD_SPEED_UP:
        /* ★ SAFETY: chỉ cho điều chỉnh khi đang chạy */
        if (!s_running) {
            ESP_LOGW(TAG, "SAFETY BLOCK: Chưa bật, bỏ qua SPEED_UP");
            return;
        }
        if (s_speed < TREADMILL_SPEED_MAX) {
            s_speed++;
            motor_set_speed(s_speed);
            ESP_LOGI(TAG, ">>> TĂNG TỐC -> %d/%d", s_speed, TREADMILL_SPEED_MAX);
        } else {
            ESP_LOGW(TAG, "Đã đạt tốc độ tối đa");
        }
        break;

    case TREADMILL_CMD_SPEED_DOWN:
        /* ★ SAFETY: chỉ cho điều chỉnh khi đang chạy */
        if (!s_running) {
            ESP_LOGW(TAG, "SAFETY BLOCK: Chưa bật, bỏ qua SPEED_DOWN");
            return;
        }
        if (s_speed > TREADMILL_SPEED_MIN) {
            s_speed--;
            if (s_speed == 0) motor_stop();
            else motor_set_speed(s_speed);
            ESP_LOGI(TAG, ">>> GIẢM TỐC -> %d/%d", s_speed, TREADMILL_SPEED_MAX);
        } else {
            ESP_LOGW(TAG, "Đã đạt tốc độ tối thiểu");
        }
        break;

    default:
        ESP_LOGW(TAG, "Lệnh không xác định: 0x%02X", cmd);
        break;
    }
}

bool treadmill_is_running(void) { return s_running; }
uint8_t treadmill_get_speed(void) { return s_speed; }
