#include "homekit.h"
#include "leds.h"

#include <string.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include "esp_log.h"
#include <nvs.h>

static const char *TAG = "homekit";

#define NVS_NAMESPACE "homekit_state"
#define KEY_ON "hk_on"
#define KEY_BRIGHTNESS "hk_bright"
#define KEY_LAST_BRIGHTNESS "hk_last_bright"
#define KEY_HUE "hk_hue"
#define KEY_SATURATION "hk_sat"

static hap_char_t
    *on_char,
    *brightness_char,
    *hue_char,
    *saturation_char;
static int32_t last_brightness;

static esp_err_t open_nvs_handle(nvs_handle_t *handle)
{
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, handle);
}

static void save_bool_nvs(nvs_handle_t nvs_handle, const char *key, bool value)
{
    if (nvs_handle)
    {
        nvs_set_u8(nvs_handle, key, (uint8_t)value);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid NVS handle to save key: %s", key);
    }
}

static void save_int32_nvs(nvs_handle_t nvs_handle, const char *key, int32_t value)
{
    if (nvs_handle)
    {
        nvs_set_i32(nvs_handle, key, value);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid NVS handle to save key: %s", key);
    }
}

static void save_float_nvs(nvs_handle_t nvs_handle, const char *key, float value)
{
    if (nvs_handle)
    {
        uint32_t u32_val;
        memcpy(&u32_val, &value, sizeof(u32_val));
        nvs_set_u32(nvs_handle, key, u32_val);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid NVS handle to save key: %s", key);
    }
}

static bool load_bool_nvs(const char *key, bool default_value)
{
    nvs_handle_t nvs_handle;
    uint8_t value_u8;
    if (open_nvs_handle(&nvs_handle) == ESP_OK)
    {
        esp_err_t err = nvs_get_u8(nvs_handle, key, &value_u8);
        nvs_close(nvs_handle);
        if (err == ESP_OK)
            return (bool)value_u8;
        if (err == ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGI(TAG, "NVS: Key '%s' not found, using default.", key);
        else
            ESP_LOGE(TAG, "NVS: Error (%s) reading key '%s'", esp_err_to_name(err), key);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS to load key: %s", key);
    }
    return default_value;
}

static int32_t load_int32_nvs(const char *key, int32_t default_value)
{
    nvs_handle_t nvs_handle;
    int32_t value;
    if (open_nvs_handle(&nvs_handle) == ESP_OK)
    {
        esp_err_t err = nvs_get_i32(nvs_handle, key, &value);
        nvs_close(nvs_handle);
        if (err == ESP_OK)
            return value;
        if (err == ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGI(TAG, "NVS: Key '%s' not found, using default.", key);
        else
            ESP_LOGE(TAG, "NVS: Error (%s) reading key '%s'", esp_err_to_name(err), key);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS to load key: %s", key);
    }
    return default_value;
}

static float load_float_nvs(const char *key, float default_value)
{
    nvs_handle_t nvs_handle;
    uint32_t u32_val;
    float value_float;
    if (open_nvs_handle(&nvs_handle) == ESP_OK)
    {
        esp_err_t err = nvs_get_u32(nvs_handle, key, &u32_val);
        nvs_close(nvs_handle);
        if (err == ESP_OK)
        {
            memcpy(&value_float, &u32_val, sizeof(value_float));
            return value_float;
        }
        if (err == ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGI(TAG, "NVS: Key '%s' not found, using default.", key);
        else
            ESP_LOGE(TAG, "NVS: Error (%s) reading key '%s'", esp_err_to_name(err), key);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to open NVS to load key: %s", key);
    }
    return default_value;
}

static void persist_all_hap_characteristics()
{
    const hap_val_t *val;
    nvs_handle_t nvs_handle;
    open_nvs_handle(&nvs_handle);

    val = hap_char_get_val(on_char);
    save_bool_nvs(nvs_handle, KEY_ON, val->b);

    val = hap_char_get_val(brightness_char);
    save_int32_nvs(nvs_handle, KEY_BRIGHTNESS, val->i);
    save_int32_nvs(nvs_handle, KEY_LAST_BRIGHTNESS, last_brightness);

    val = hap_char_get_val(hue_char);
    save_float_nvs(nvs_handle, KEY_HUE, val->f);

    val = hap_char_get_val(saturation_char);
    save_float_nvs(nvs_handle, KEY_SATURATION, val->f);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

static int ws2812_read(hap_char_t *hc, hap_status_t *status_code,
                       void *serv_priv, void *read_priv)
{
    ESP_LOGI(TAG, "Read callback invoked for characteristic: %s", hap_char_get_type_uuid(hc));

    const char *char_uuid = hap_char_get_type_uuid(hc);
    const hap_val_t *current_val_ptr;
    *status_code = HAP_STATUS_SUCCESS;

    if (strcmp(char_uuid, HAP_CHAR_UUID_ON) == 0 ||
        strcmp(char_uuid, HAP_CHAR_UUID_BRIGHTNESS) == 0 ||
        strcmp(char_uuid, HAP_CHAR_UUID_HUE) == 0 ||
        strcmp(char_uuid, HAP_CHAR_UUID_SATURATION) == 0 ||
        strcmp(char_uuid, HAP_CHAR_UUID_NAME) == 0)
    {

        current_val_ptr = hap_char_get_val(hc);
        if (current_val_ptr)
        {
            return HAP_SUCCESS;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get value for characteristic: %s", char_uuid);
            *status_code = HAP_STATUS_RES_ABSENT;
            return HAP_FAIL;
        }
    }
    else
    {
        ESP_LOGW(TAG, "Read for unhandled characteristic UUID: %s", char_uuid);
        *status_code = HAP_STATUS_RES_ABSENT;
        return HAP_FAIL;
    }
}

static int ws2812_write(hap_write_data_t write_data[], int count,
                        void *serv_priv, void *write_priv)
{
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    bool state_changed = false;
    for (i = 0; i < count; i++)
    {
        write = &write_data[i];
        *(write->status) = HAP_STATUS_VAL_INVALID;
        const char *char_uuid = hap_char_get_type_uuid(write->hc);
        if (!strcmp(char_uuid, HAP_CHAR_UUID_ON))
        {
            bool new_on = write->val.b;
            if (!new_on)
            {
                last_brightness = hap_char_get_val(brightness_char)->i;
                hap_val_t zero = {.i = 0};
                hap_char_update_val(brightness_char, &zero);
            }
            else
            {
                hap_val_t v = {.i = last_brightness};
                hap_char_update_val(brightness_char, &v);
            }

            hap_char_update_val(on_char, &write->val);
            *(write->status) = HAP_STATUS_SUCCESS;
            state_changed = true;
        }
        else if (!strcmp(char_uuid, HAP_CHAR_UUID_BRIGHTNESS))
        {
            int32_t new_b = write->val.i;

            if (new_b > 0)
            {
                last_brightness = new_b;
                hap_val_t onv = {.b = true};
                hap_char_update_val(on_char, &onv);
            }
            else
            {
                hap_val_t offv = {.b = false};
                hap_char_update_val(on_char, &offv);
            }

            hap_char_update_val(brightness_char, &write->val);
            *(write->status) = HAP_STATUS_SUCCESS;
            state_changed = true;
        }
        else if (!strcmp(char_uuid, HAP_CHAR_UUID_HUE))
        {
            hap_char_update_val(hue_char, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
            state_changed = true;
        }
        else if (!strcmp(char_uuid, HAP_CHAR_UUID_SATURATION))
        {
            hap_char_update_val(saturation_char, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
            state_changed = true;
        }
        else
        {
            ESP_LOGI(TAG, "Write for unhandled characteristic UUID: %s", char_uuid);
            *(write->status) = HAP_FAIL;
        }

        if (*(write->status) != HAP_STATUS_SUCCESS)
        {
            ret = HAP_FAIL;
        }
    }

    if (ret == HAP_SUCCESS && state_changed)
    {
        persist_all_hap_characteristics();
    }

    return ret;
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
        .serial_num = "666",
        .fw_rev = "1.0.1",
        .hw_rev = NULL,
        .pv = "1.1",
        .identify_routine = accessory_identify_routine,
        .cid = HAP_CID_LIGHTING};

    hap_acc_t *accessory = hap_acc_create(&cfg);
    hap_add_accessory(accessory);

    hap_acc_add_wifi_transport_service(accessory, 0);

    hap_serv_t *light_service = hap_serv_lightbulb_create(true);
    hap_serv_add_char(light_service, hap_char_name_create("ESP32 Lamp"));

    int32_t initial_brightness = load_int32_nvs(KEY_LAST_BRIGHTNESS, load_int32_nvs(KEY_BRIGHTNESS, 100));
    float initial_hue = load_float_nvs(KEY_HUE, 0.0f);
    float initial_saturation = load_float_nvs(KEY_SATURATION, 0.0f);

    on_char = hap_serv_get_char_by_uuid(light_service, HAP_CHAR_UUID_ON);
    brightness_char = hap_char_brightness_create(initial_brightness);
    hue_char = hap_char_hue_create(initial_hue);
    saturation_char = hap_char_saturation_create(initial_saturation);

    hap_serv_add_char(light_service, brightness_char);
    hap_serv_add_char(light_service, hue_char);
    hap_serv_add_char(light_service, saturation_char);

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
    ws2812_set_hue(val->f);

    val = hap_char_get_val(saturation_char);
    ws2812_set_saturation(val->f);
}

void led_sync_task(void *pvParameter)
{
    while (1)
    {
        sync_leds_to_homekit();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
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

    hap_http_debug_enable();
    ret = hap_start();
    if (ret != HAP_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to start HomeKit");
        return HAP_FAIL;
    }
    ESP_LOGI(TAG, "HomeKit started");

    ESP_LOGI(TAG, "Starting LED sync task");
    xTaskCreate(led_sync_task, "LED Sync Task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    ESP_LOGI(TAG, "Started LED sync task");

    hap_val_t onv = {.b = true};
    hap_char_update_val(on_char, &onv);
    return HAP_SUCCESS;
}
