#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "esp_system.h"
#include "esp_log.h"

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "homekit.h"
#include "wifi.h"
#include "leds.h"

static const char *TAG = "main";

void initialize_homekit()
{
    ESP_LOGI(TAG, "Starting HomeKit");
    if (start_homekit() != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to start HomeKit");
        return;
    }
    ESP_LOGI(TAG, "Started HomeKit");
}

void start_wifi()
{

    ESP_LOGI(TAG, "Initializing WiFi");
    ESP_ERROR_CHECK(app_wifi_init());
    ESP_LOGI(TAG, "Initialized WiFi");

    ESP_LOGI(TAG, "Starting WiFi");
    ESP_ERROR_CHECK(app_wifi_start(10, initialize_homekit));
    ESP_LOGI(TAG, "Started WiFi");
}

void initialize_leds()
{
    ESP_LOGI(TAG, "Starting LEDs");
    ws2812_init();
    ESP_LOGI(TAG, "Started LEDs");
}

void app_main(void)
{
    initialize_leds();
    start_wifi();
}
