idf_component_register(
    SRCS 
        "src/scd4x_i2c.c"
        "src/sensirion_common.c"
        "src/sensirion_i2c.c"
        "src/sensirion_i2c_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
