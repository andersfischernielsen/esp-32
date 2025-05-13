#include "homekit.h"
#include "leds.h"

#include <string.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "esp_log.h"

static const char *TAG = "homekit";

static hap_char_t
    *on_char,
    *brightness_char,
    *hue_char,
    *saturation_char;

static int ws2812_write(hap_write_data_t write_data[], int count,
                        void *serv_priv, void *write_priv)
{
    ESP_LOGI(TAG, "Light write callback called");
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++)
    {
        write = &write_data[i];
        *(write->status) = HAP_STATUS_VAL_INVALID;
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON))
        {
            ws2812_set_power(write->val.b);
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_BRIGHTNESS))
        {
            ws2812_set_brightness(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_HUE))
        {
            ws2812_set_hue(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_SATURATION))
        {
            ws2812_set_saturation(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        }
        else
        {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }

        if (*(write->status) == HAP_STATUS_SUCCESS)
        {
            hap_char_update_val(write->hc, &(write->val));
        }
        else
        {
            ret = HAP_FAIL;
        }
    }
    return ret;
}

static int ws2812_read(
    hap_char_t *hc,
    hap_status_t *status_code,
    void *serv_priv,
    void *read_priv)
{
    ESP_LOGI(TAG, "Light read callback called");
    hap_val_t val;
    const char *uuid = hap_char_get_type_uuid(hc);

    *status_code = HAP_STATUS_RES_ABSENT;
    if (!strcmp(uuid, HAP_CHAR_UUID_ON))
    {
        val.b = ws2812_get_power();
        ESP_LOGI(TAG, "Power: %d", val.b);
        hap_char_update_val(hc, &val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    else if (!strcmp(uuid, HAP_CHAR_UUID_BRIGHTNESS))
    {
        val.i = ws2812_get_brightness();
        ESP_LOGI(TAG, "Brightness: %d", val.i);
        hap_char_update_val(hc, &val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    else if (!strcmp(uuid, HAP_CHAR_UUID_HUE))
    {
        val.f = ws2812_get_hue();
        ESP_LOGI(TAG, "Hue: %f", val.f);
        hap_char_update_val(hc, &val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    else if (!strcmp(uuid, HAP_CHAR_UUID_SATURATION))
    {
        val.f = ws2812_get_saturation();
        ESP_LOGI(TAG, "Saturation: %f", val.f);
        hap_char_update_val(hc, &val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    else if (!strcmp(uuid, HAP_CHAR_UUID_NAME))
    {
        val.s = "ESP32 Lamp";
        *status_code = HAP_STATUS_SUCCESS;
    }
    else
    {
        return HAP_FAIL;
    }

    hap_char_update_val(hc, &val);

    return *status_code == HAP_STATUS_SUCCESS;
}

int accessory_identify_routine(hap_acc_t *accessory)
{
    ESP_LOGI(TAG, "Accessory identify called");
    return HAP_SUCCESS;
}

int create_accessories_and_services(void)
{
    hap_acc_cfg_t cfg = {
        .name = "ESP32 Lamp",
        .manufacturer = "Espressif",
        .model = "ESP32 Lamp",
        .serial_num = "001",
        .fw_rev = "1.0.0",
        .hw_rev = NULL,
        .pv = "1.1",
        .identify_routine = accessory_identify_routine,
        .cid = HAP_CID_LIGHTING};

    hap_acc_t *accessory = hap_acc_create(&cfg);
    hap_add_accessory(accessory);

    hap_acc_add_wifi_transport_service(accessory, 0);

    hap_serv_t *light_service = hap_serv_lightbulb_create(false);
    hap_serv_add_char(light_service, hap_char_name_create("LED Light"));
    on_char = hap_char_on_create(false);
    brightness_char = hap_char_brightness_create(50);
    hue_char = hap_char_hue_create(0);
    saturation_char = hap_char_saturation_create(0);

    hap_serv_set_write_cb(light_service, ws2812_write);
    hap_serv_set_read_cb(light_service, ws2812_read);

    hap_acc_add_serv(accessory, light_service);

    return HAP_SUCCESS;
}

void sync_leds_to_homekit(void)
{
    const hap_val_t *val;

    val = hap_char_get_val(on_char);
    ws2812_set_power(val->b);

    val = hap_char_get_val(brightness_char);
    ws2812_set_brightness(val->i);

    val = hap_char_get_val(hue_char);
    ws2812_set_hue((int)val->f);

    val = hap_char_get_val(saturation_char);
    ws2812_set_saturation((int)val->f);
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

    sync_leds_to_homekit();
    ESP_LOGI(TAG, "LEDs synced to HomeKit");

    return HAP_SUCCESS;
}