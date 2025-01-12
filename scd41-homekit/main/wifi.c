/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * This supports 3 ways of connecting to the Wi-Fi Network
 * 1. Hard coded credentials
 * 2. Unified Provisioning
 * 3. Apple Wi-Fi Accessory Configuration (WAC. Available only on MFi variant of the SDK)
 *
 * Unified Provisioning has 2 options
 * 1. BLE Provisioning
 * 2. SoftAP Provisioning
 *
 * Unified and WAC Provisioning can co-exist.
 * If the SoftAP Unified Provisioning is being used, the same SoftAP interface
 * will be used for WAC. However, if BLE Unified Provisioning is used, the HAP
 * Helper functions for softAP start/stop will be used
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_idf_version.h>
#include "sdkconfig.h"

#define APP_WIFI_SSID CONFIG_WIFI_SSID
#define APP_WIFI_PASS CONFIG_WIFI_PASSWORD

#include <esp_netif.h>

#include <wifi_provisioning/manager.h>

#include <nvs.h>
#include <nvs_flash.h>
#include "wifi.h"

static const char *TAG = "wifi";
static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        esp_netif_create_ip6_linklocal((esp_netif_t *)arg);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IPv4: " IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6)
    {
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
}

void app_wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();

    // Handle situations where partition is truncated or new partition table is in use
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Erase then re-init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize TCP/IP */
    esp_netif_init();

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_t *wifi_netif = esp_netif_create_default_wifi_sta();

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, wifi_netif));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

esp_err_t app_wifi_start(TickType_t ticks_to_wait)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = APP_WIFI_SSID,
            .password = APP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, ticks_to_wait);
    return ESP_OK;
}
