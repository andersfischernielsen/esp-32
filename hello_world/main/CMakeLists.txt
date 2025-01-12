idf_component_register(
    SRCS "hello_world_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        spi_flash
        esp_driver_gpio
        esp_driver_i2c
        i2c-scd4x
        esp_hap_apple_profiles
        esp_hap_core
        esp_hap_extras
        esp_hap_platform
        hkdf-sha
        json_generator
        json_parser
        mu_srp
)
