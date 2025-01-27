#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
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

#include "ld2420.h"
#include "homekit.h"
#include "wifi.h"

static const char *TAG = "app";

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

#define LD2420_UART_NUM UART_NUM_1
#define LD2420_UART_RX_PIN (GPIO_NUM_16)
#define LD2420_UART_TX_PIN (GPIO_NUM_17)
#define LD2420_UART_BAUD_RATE 256000
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

static esp_err_t ld2420_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = LD2420_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(LD2420_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LD2420_UART_NUM,
                                 LD2420_UART_TX_PIN,
                                 LD2420_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    return uart_driver_install(LD2420_UART_NUM,
                               LD2420_UART_RX_BUF_SIZE,
                               0,
                               0,
                               NULL,
                               0);
}

static void ld2420_uart_task(void *arg)
{
    uint8_t data[128];
    char line_buf[256];
    int line_pos = 0;

    while (1)
    {
        int len = uart_read_bytes(LD2420_UART_NUM, data, sizeof(data), pdMS_TO_TICKS(100));
        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                char c = (char)data[i];
                if (c == '\n' || c == '\r')
                {
                    if (line_pos > 0)
                    {
                        line_buf[line_pos] = '\0';
                        bool occupancy = ld2420_parse_simple_mode(line_buf, line_pos);
                        update_hap_occupancy(occupancy);
                        line_pos = 0;
                    }
                }
                else
                {
                    if (line_pos < (int)sizeof(line_buf) - 1)
                    {
                        line_buf[line_pos++] = c;
                    }
                    else
                    {
                        line_pos = 0;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void start_ld2420_task(void)
{
    xTaskCreate(ld2420_uart_task, "ld2420_uart_task", 2048, NULL, 5, NULL);
}

static void scd4x_i2c_task(void *arg)
{
    while (1)
    {
        uint16_t raw_co2;
        int32_t raw_temperature, raw_humidity;
        bool data_ready = false;

        esp_err_t ret = scd4x_get_data_ready_flag(&data_ready);
        if (ret == 0 && data_ready)
        {
            ret = scd4x_read_measurement(&raw_co2, &raw_temperature, &raw_humidity);
            if (ret == 0)
            {
                float temperature = raw_temperature / 1000.0f;
                float humidity = raw_humidity / 1000.0f;
                float co2 = (float)raw_co2;
                ret = update_hap_values(temperature, humidity, co2);
                if (ret != 0)
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

        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

static void start_scd4x_task(void)
{
    xTaskCreate(scd4x_i2c_task, "scd4x_i2c_task", 2048, NULL, 5, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing WiFi");
    ESP_ERROR_CHECK(app_wifi_init());

    ESP_LOGI(TAG, "Starting WiFi");
    ESP_ERROR_CHECK(app_wifi_start(2));

    ESP_LOGI(TAG, "Initializing I2C & SCD4x");
    ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(TAG, "Initializing UART & LD2420");
    ESP_ERROR_CHECK(ld2420_uart_init());

    ESP_LOGI(TAG, "Stopping any ongoing measurements");
    ESP_ERROR_CHECK(scd4x_stop_periodic_measurement());

    ESP_LOGI(TAG, "Waiting for sensor to initialize");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Starting SCD4X periodic measurement");
    ESP_ERROR_CHECK(scd4x_start_periodic_measurement());

    ESP_LOGI(TAG, "Waiting for first measurement");
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "Starting HomeKit");
    if (start_homekit() != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to start HomeKit");
        return;
    }

    start_scd4x_task();
    start_ld2420_task();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
