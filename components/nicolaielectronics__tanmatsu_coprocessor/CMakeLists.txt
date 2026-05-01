idf_component_register(
    SRCS
        "tanmatsu_coprocessor.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        "esp_driver_i2c"
        "esp_driver_gpio"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    TANMATSU_COPROCESSOR_VER_MAJOR=0
    TANMATSU_COPROCESSOR_VER_MINOR=7
    TANMATSU_COPROCESSOR_VER_PATCH=0
)
