#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

typedef void (*wifi_ready_cb_t)(void);
esp_err_t app_wifi_init(void);
esp_err_t app_wifi_start(TickType_t ticks_to_wait, wifi_ready_cb_t cb);
