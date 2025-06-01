#include "homekit.h"

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "esp_log.h"

static const char *TAG = "homekit";

static hap_char_t *g_temp_char = NULL;
static hap_char_t *g_humidity_char = NULL;
static hap_char_t *g_co2_detected_char = NULL;
static hap_char_t *g_co2_level_char = NULL;

int accessory_identify_routine(hap_acc_t *accessory)
{
    ESP_LOGI(TAG, "Accessory identify called");
    return HAP_SUCCESS;
}

int update_hap_climate(float temperature, float humidity, float co2)
{
    if (g_temp_char)
    {
        ESP_LOGI(TAG, "Temperature: %.2f °C", temperature);
        hap_val_t temperature_val = {.f = temperature};
        ESP_LOGI(TAG, "Setting temperature to %.2f", temperature);
        hap_char_update_val(g_temp_char, &temperature_val);
    }

    if (g_humidity_char)
    {
        ESP_LOGI(TAG, "Humidity: %.2f %%RH", humidity);
        hap_val_t humidity_val = {.f = humidity};
        ESP_LOGI(TAG, "Setting humidity to %.2f", humidity);
        hap_char_update_val(g_humidity_char, &humidity_val);
    }

    if (g_co2_detected_char)
    {
        ESP_LOGI(TAG, "CO2: %.1f ppm", co2);
        int co2_detected = (co2 > 1000) ? 1 : 0;
        hap_val_t detected_val = {.i = co2_detected};
        hap_val_t level_val = {.f = co2};
        ESP_LOGI(TAG, "Setting CO2 detected to %d", co2_detected);
        hap_char_update_val(g_co2_detected_char, &detected_val);
        hap_char_update_val(g_co2_level_char, &level_val);
    }

    return HAP_SUCCESS;
}

int create_accessories_and_services(void)
{
    hap_acc_cfg_t cfg = {
        .name = "ESP32 Climate Sensor",
        .manufacturer = "Espressif",
        .model = "ESP32 Climate Sensor",
        .serial_num = "666",
        .fw_rev = "1.0.0",
        .hw_rev = NULL,
        .pv = "1.1",
        .identify_routine = accessory_identify_routine,
        .cid = HAP_CID_SENSOR};

    hap_acc_t *accessory = hap_acc_create(&cfg);
    hap_add_accessory(accessory);

    hap_serv_t *temperature_service = hap_serv_temperature_sensor_create(22.0F);
    g_temp_char = hap_serv_get_char_by_uuid(temperature_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);

    hap_serv_t *hum_service = hap_serv_humidity_sensor_create(20.0F);
    g_humidity_char = hap_serv_get_char_by_uuid(hum_service, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);

    hap_serv_t *co2_service = hap_serv_carbon_dioxide_sensor_create(0);
    g_co2_detected_char = hap_serv_get_char_by_uuid(co2_service, HAP_CHAR_UUID_CARBON_DIOXIDE_DETECTED);
    g_co2_level_char = hap_char_carbon_dioxide_level_create(400.0F);
    hap_serv_add_char(co2_service, g_co2_level_char);

    hap_acc_add_serv(accessory, hum_service);
    hap_acc_add_serv(accessory, temperature_service);
    hap_acc_add_serv(accessory, co2_service);

    hap_acc_add_wifi_transport_service(accessory, 0);

    return HAP_SUCCESS;
}

int start_homekit(void)
{
    hap_set_setup_code("347-53-475");
    hap_set_setup_id("3457");

    int ret = hap_init(HAP_TRANSPORT_WIFI);
    if (ret != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to initialize HomeKit");
        return HAP_FAIL;
    }

    hap_delete_all_accessories();

    ret = create_accessories_and_services();
    if (ret != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to create accessory and services");
        return HAP_FAIL;
    }

    ret = hap_start();
    if (ret != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to start HomeKit");
        return HAP_FAIL;
    }
    ESP_LOGI(TAG, "HomeKit started");
    return HAP_SUCCESS;
}