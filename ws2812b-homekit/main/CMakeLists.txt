idf_component_register(
    SRCS 
        "main.c"
        "wifi.c"
        "homekit.c"
        "leds.c"
       
    INCLUDE_DIRS "."
    PRIV_REQUIRES 
        esp_hap_apple_profiles
        esp_hap_core
        esp_hap_extras
        esp_hap_platform
        hkdf-sha
        json_generator
        json_parser
        mu_srp
        esp_wifi
        esp_netif
        nvs_flash
        wifi_provisioning
        driver
)
