#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "driver/uart.h"
#include "driver/i2c.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"

#include "homekit.h"
#include "wifi.h"

static const char *TAG = "main";

#define I2C_MASTER_SCL_IO 7
#define I2C_MASTER_SDA_IO 6
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define LD2420_UART_RX_PIN (GPIO_NUM_16)
#define LD2420_UART_TX_PIN (GPIO_NUM_17)

#define LD2420_UART_RX_BUF_SIZE 1024

i2c_port_t i2c_master_port = I2C_MASTER_NUM;

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void scd4x_i2c_task(void *arg)
{
    while (1)
    {
        static uint16_t raw_co2;
        static int32_t raw_temperature, raw_humidity;
        static bool data_ready = false;

        esp_err_t ret = scd4x_get_data_ready_flag(&data_ready);
        if (ret == 0 && data_ready)
        {
            ret = scd4x_read_measurement(&raw_co2, &raw_temperature, &raw_humidity);
            if (ret == 0)
            {
                float temperature = (raw_temperature / 1000.0f) - 7.0f;
                ESP_LOGI(TAG, "Raw Temperature: %" PRId32 ", Adjusted Temperature: %.2f °C", raw_temperature, temperature);
                float humidity = raw_humidity / 1000.0f;
                float co2 = (float)raw_co2;
                ret = update_hap_climate(temperature, humidity, co2);
                if (ret != HAP_SUCCESS)
                {
                    ESP_LOGE(TAG, "Failed to update HomeKit values");
                }
            }
            else
            {
                ESP_LOGW(TAG, "Failed to read measurement");
            }
        }
        else if (ret != 0)
        {
            ESP_LOGW(TAG, "Failed to get data ready flag");
        }
        else
        {
            ESP_LOGI(TAG, "Data not ready yet");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void start_i2c_sdc4x()
{
    ESP_LOGI(TAG, "Initializing I2C & SCD4x");
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "Initialized I2C & SCD4x");

    ESP_LOGI(TAG, "Waiting for sensor to initialize");
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Waited for sensor to initialize");

    ESP_LOGI(TAG, "Stopping any ongoing measurements");
    ESP_ERROR_CHECK(scd4x_stop_periodic_measurement());
    ESP_LOGI(TAG, "Stopped any ongoing measurements");

    ESP_LOGI(TAG, "Starting SCD4X periodic measurement");
    ESP_ERROR_CHECK(scd4x_start_periodic_measurement());
    ESP_LOGI(TAG, "Started SCD4X periodic measurement");

    ESP_LOGI(TAG, "Waiting for first measurement");
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "Waited for first measurement");

    ESP_LOGI(TAG, "Starting SCD4X task");
    xTaskCreate(scd4x_i2c_task, "scd4x_i2c_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Started SCD4X task");
}

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

void app_main(void)
{
    start_i2c_sdc4x();
    start_wifi();
}
