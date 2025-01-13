idf_component_register(
    SRCS 
        "main.c"
        "wifi.c"
        "matter.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        spi_flash
        esp_driver_gpio
        esp_driver_i2c
        i2c-scd4x
        esp_wifi
        esp_netif
        nvs_flash
        wifi_provisioning
)
set(CMAKE_CXX_STANDARD 17)
set_source_files_properties(matter.cpp PROPERTIES LANGUAGE CXX)
