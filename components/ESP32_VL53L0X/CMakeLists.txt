# NOTE (2026-10-24, vendoring): this file is the ONLY vendored file
# modified from upstream kerikun11/ESP32_VL53L0X. The original used
# the legacy register_component() API, which ESP-IDF 5.x removed, so
# a fresh clone would not build on IDF 5. Converted to the
# idf_component_register() API (available since IDF 4.0); the library
# sources, headers, compile definitions and options are unchanged.
idf_component_register(
    SRCS
        api/core/src/vl53l0x_api.c
        api/core/src/vl53l0x_api_calibration.c
        api/core/src/vl53l0x_api_core.c
        api/core/src/vl53l0x_api_ranging.c
        api/core/src/vl53l0x_api_strings.c
        api/platform/src/vl53l0x_platform.c
        api/platform/src/vl53l0x_platform_log.c
    INCLUDE_DIRS
        inc
        api/core/inc
        api/platform/inc
    REQUIRES
        driver
)

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE USE_I2C_2V8=1
)

target_compile_options(${COMPONENT_LIB}
    PRIVATE -mlongcalls
)
