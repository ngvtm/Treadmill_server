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
    INCLUDE_DIRS
        "."
        "treadmill"                 # <-- thêm dòng này để #include "treadmill.h" hoạt động
        "motor"
    REQUIRES
        driver
        esp_timer
        nvs_flash
        bt               
        freertos
        log
        esp_system
)
                     
