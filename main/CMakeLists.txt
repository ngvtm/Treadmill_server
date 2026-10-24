# set(srcs "main.c"
#          "gatt_svr.c")

# idf_component_register(SRCS "${srcs}"
#                        INCLUDE_DIRS ".")

idf_component_register(
    SRCS
        "main.c"
        "gatt_svr.c"
        "treadmill/treadmill.c"     # <-- thêm dòng này
        "motor/motor.c"
        "ir_safety/ir_safety.cpp"
        "safety_key/safety_key.c"
        "motor_feedback/motor_feedback.c"
        "settings/settings.c"
        "blackbox/blackbox.c"
    INCLUDE_DIRS
        "."
        "treadmill"                 # <-- thêm dòng này để #include "treadmill.h" hoạt động
        "motor"
        "ir_safety"
        "safety_key"
        "motor_feedback"
        "settings"
        "blackbox"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        bt               
        freertos
        log
        esp_system
        ESP32_VL53L0X
)
                     
