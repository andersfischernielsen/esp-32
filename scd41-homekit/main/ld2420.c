#include "ld2420.h"
#include "homekit.h"
#include <math.h>

#define NO_OCCUPANCY_DELAY_MS 60000

#define SENSOR_READ_INTERVAL_MS 100

static const char *TAG = "ld2420";

static uint16_t occupancy_detected = 0;
static uint16_t distance_detected = 0;
static TickType_t last_occupancy_time = 0;

esp_err_t ld2420_send_raw(const uint8_t *frame, size_t len)
{
    if (len > 64)
    {
        ESP_LOGE(TAG, "Frame length %d exceeds limit of 64 bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOG_BUFFER_HEX(TAG, frame, len);
    int written = uart_write_bytes(LD2420_UART, (const char *)frame, len);
    if (written < 0)
    {
        ESP_LOGE(TAG, "UART write error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ld2420_send_report_mode(void)
{
    uint8_t frame[] = {
        0xFD, 0xFC, 0xFB, 0xFA,
        0x08, 0x00,
        0x12, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x04, 0x03, 0x02, 0x01};
    return ld2420_send_raw(frame, sizeof(frame));
}

void process_sensor_frame(const uint8_t *buffer, int length)
{
    if (length < 45)
    {
        ESP_LOGW(TAG, "Energy frame too short. Size=%d", length);
        return;
    }

    uint16_t distance_cm = 0;
    memcpy(&distance_cm, &buffer[7], sizeof(distance_cm));
    distance_detected = distance_cm;
    uint16_t gates[16];

    int idx = 9;
    for (int i = 0; i < 16; i++)
    {
        memcpy(&gates[i], &buffer[idx], sizeof(uint16_t));
        idx += 2;
    }

    if (distance_cm > 40)
    {
        occupancy_detected = 1;
        last_occupancy_time = xTaskGetTickCount();
    }
    else
    {
        occupancy_detected = 0;
    }
}

void ld2420_read_task(void *param)
{
    static uint8_t data[256];
    static uint8_t local_buf[256 / 2];
    int pos = 0;

    while (1)
    {
        int len = uart_read_bytes(LD2420_UART, data, sizeof(data), pdMS_TO_TICKS(1000));
        if (len > 0)
        {
            if (pos + len > (int)sizeof(local_buf))
            {
                pos = 0;
            }
            memcpy(&local_buf[pos], data, len);
            pos += len;

            while (pos >= 6)
            {
                if (local_buf[0] != 0xF4 || local_buf[1] != 0xF3 ||
                    local_buf[2] != 0xF2 || local_buf[3] != 0xF1)
                {
                    memmove(local_buf, local_buf + 1, pos - 1);
                    pos -= 1;
                    continue;
                }

                if (pos < 6)
                {
                    break;
                }

                uint16_t payload_length = local_buf[4] | (local_buf[5] << 8);
                uint16_t frame_length = payload_length + 10;
                if (frame_length < 13 || frame_length > 64)
                {
                    memmove(local_buf, local_buf + 1, pos - 1);
                    pos -= 1;
                    continue;
                }

                if (pos < frame_length)
                {
                    break;
                }

                if (local_buf[frame_length - 4] != 0xF8 ||
                    local_buf[frame_length - 3] != 0xF7 ||
                    local_buf[frame_length - 2] != 0xF6 ||
                    local_buf[frame_length - 1] != 0xF5)
                {
                    memmove(local_buf, local_buf + 1, pos - 1);
                    pos -= 1;
                    continue;
                }

                process_sensor_frame(local_buf, frame_length);

                if (pos > frame_length)
                {
                    memmove(local_buf, local_buf + frame_length, pos - frame_length);
                }
                pos -= frame_length;
            }
        }

        if (occupancy_detected == 0 &&
            ((xTaskGetTickCount() - last_occupancy_time) >= pdMS_TO_TICKS(NO_OCCUPANCY_DELAY_MS)))
        {
            update_hap_occupancy(0);
        }
        else if (occupancy_detected == 1)
        {
            update_hap_occupancy(1);
        }

        ESP_LOGI(TAG, "Distance: %u cm", distance_detected);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
    vTaskDelete(NULL);
}
