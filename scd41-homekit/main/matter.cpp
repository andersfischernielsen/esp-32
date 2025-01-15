#include "matter.h"

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_endpoint.h>

static const char *TAG = "matter";

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static endpoint_t *g_temp_sensor_ep = nullptr;
static endpoint_t *g_humidity_sensor_ep = nullptr;
static endpoint_t *g_air_quality_ep = nullptr;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type)
    {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed successfully");
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;

    default:
        break;
    }
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                         uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id,
                                         esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

int initialize_matter()
{
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, nullptr);
    if (!node)
    {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return ESP_FAIL;
    }

    {
        temperature_sensor::config_t temp_sensor_config;
        g_temp_sensor_ep = temperature_sensor::create(node, &temp_sensor_config, ENDPOINT_FLAG_NONE, nullptr);
        if (!g_temp_sensor_ep)
        {
            ESP_LOGE(TAG, "Failed to create temperature sensor endpoint");
        }
    }

    {
        humidity_sensor::config_t humidity_sensor_config;
        g_humidity_sensor_ep = humidity_sensor::create(node, &humidity_sensor_config, ENDPOINT_FLAG_NONE, nullptr);
        if (!g_humidity_sensor_ep)
        {
            ESP_LOGE(TAG, "Failed to create humidity sensor endpoint");
        }
    }

    {
        air_quality_sensor::config_t air_quality_sensor_config;
        g_air_quality_ep = air_quality_sensor::create(node, &air_quality_sensor_config, ENDPOINT_FLAG_NONE, nullptr);
        if (!g_air_quality_ep)
        {
            ESP_LOGE(TAG, "Failed to create air quality endpoint");
        }
    }

    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_matter::start() failed: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "Matter stack started, endpoints created");
    return ESP_OK;
}

int update_matter_values(float temperature, float humidity, float co2)
{
    int16_t scaled_temp = static_cast<int16_t>(temperature * 100);
    uint16_t scaled_hum = static_cast<uint16_t>(humidity * 100);
    uint16_t scaled_co2 = static_cast<uint16_t>(co2);

    uint16_t temp_ep_id = endpoint::get_id(g_temp_sensor_ep);
    uint16_t hum_ep_id = endpoint::get_id(g_humidity_sensor_ep);
    uint16_t co2_ep_id = endpoint::get_id(g_air_quality_ep);

    if (temp_ep_id > 0)
    {
        esp_matter_attr_val_t tval = esp_matter_int16(scaled_temp);
        esp_err_t err = attribute::update(temp_ep_id, TemperatureMeasurement::Id,
                                          TemperatureMeasurement::Attributes::MeasuredValue::Id, &tval);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to update temperature attribute: %d", err);
        }
    }

    if (hum_ep_id > 0)
    {
        esp_matter_attr_val_t hval = esp_matter_uint16(scaled_hum);
        esp_err_t err = attribute::update(hum_ep_id, RelativeHumidityMeasurement::Id,
                                          RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &hval);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to update humidity attribute: %d", err);
        }
    }

    if (co2_ep_id > 0)
    {
        esp_matter_attr_val_t cval = esp_matter_uint16(scaled_co2);
        esp_err_t err = attribute::update(co2_ep_id, CarbonDioxideConcentrationMeasurement::Id,
                                          CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, &cval);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to update CO2 attribute: %d", err);
        }
    }

    ESP_LOGI(TAG, "Updated Matter values: T=%.2f°C, RH=%.2f%%, CO2=%.2f ppm", temperature, humidity, co2);
    return ESP_OK;
}
