#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"

#include "matter.h"
#include "wifi.h"

#define TAG "app"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

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

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C & SCD4x");
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C master");
        return;
    }

    ESP_LOGI(TAG, "Initializing Sensirion HAL");
    sensirion_i2c_hal_init();

    app_wifi_init();
    app_wifi_start(pdMS_TO_TICKS(10000));

    ESP_LOGI(TAG, "Stopping any ongoing measurements");
    ret = scd4x_stop_periodic_measurement();
    if (ret != 0)
    {
        ESP_LOGW(TAG, "Failed to stop periodic measurement");
    }

    ESP_LOGI(TAG, "Waiting for sensor to initialize");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting SCD4X periodic measurement");
    ret = scd4x_start_periodic_measurement();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to start periodic measurement");
        return;
    }

    ESP_LOGI(TAG, "Waiting for first measurement");
    vTaskDelay(pdMS_TO_TICKS(5000));
    initialize_matter();

    while (1)
    {
        uint16_t raw_co2;
        int32_t raw_temperature, raw_humidity;
        bool data_ready = false;

        ret = scd4x_get_data_ready_flag(&data_ready);
        if (ret == 0 && data_ready)
        {
            ret = scd4x_read_measurement(&raw_co2, &raw_temperature, &raw_humidity);
            if (ret == 0)
            {
                float temperature = raw_temperature / 1000.0f;
                float humidity = raw_humidity / 1000.0f;
                float co2 = (float)raw_co2;
                update_matter_values(temperature, humidity, co2);
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

        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}
