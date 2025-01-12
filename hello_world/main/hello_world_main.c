/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai)
 * CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"

#include <hap.h>

#define TAG "SCD4X_APP"

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

void i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Device found at address 0x%02X\n", addr);
        }
    }
    ESP_LOGI(TAG, "I2C scan completed.");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C Master");
    esp_err_t ret = i2c_master_init();
    i2c_scan();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C master: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Initializing Sensirion HAL");
    sensirion_i2c_hal_init();

    ESP_LOGI(TAG, "Stopping any ongoing measurements...");
    ret = scd4x_stop_periodic_measurement();
    if (ret != 0)
    {
        ESP_LOGW(TAG, "Failed to stop periodic measurement: %d", ret);
    }

    ESP_LOGI(TAG, "Waiting for sensor to initialize...");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Ensure sensor is ready

    ESP_LOGI(TAG, "Starting SCD4X periodic measurement");
    ret = scd4x_start_periodic_measurement();
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to start periodic measurement: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "Waiting for first measurement...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1)
    {
        uint16_t co2;
        int32_t temperature, humidity;
        bool data_ready = false;

        ret = scd4x_get_data_ready_flag(&data_ready);
        if (ret == 0 && data_ready)
        {
            ret = scd4x_read_measurement(&co2, &temperature, &humidity);
            if (ret == 0)
            {
                ESP_LOGI(TAG, "CO2: %u ppm, Temperature: %.2f °C, Humidity: %.2f %%RH",
                         co2, temperature / 1000.0, humidity / 1000.0);
            }
            else
            {
                ESP_LOGW(TAG, "Failed to read measurement: %d", ret);
            }
        }
        else if (ret != 0)
        {
            ESP_LOGW(TAG, "Failed to get data ready flag: %d", ret);
        }
        else
        {
            ESP_LOGI(TAG, "Data not ready yet.");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
