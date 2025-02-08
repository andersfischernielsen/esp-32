#include "ld2420.h"
#include "homekit.h"
#include <math.h>

static const char *TAG = "ld2420";

static const float decay_factor = 0.000035f; // Decay factor for EMA (example: ~1/28800 for once-per-second over 8 hours)
static const float outlier_threshold = 3.0f; // Outlier threshold in terms of standard deviations

static float gate_means[16] = {0};
static float gate_vars[16] = {0};
static bool stats_initialized = false;

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
    uint16_t gates[16];

    int idx = 9;
    for (int i = 0; i < 16; i++)
    {
        memcpy(&gates[i], &buffer[idx], sizeof(uint16_t));
        idx += 2;
    }

    if (!stats_initialized)
    {
        for (int i = 0; i < 16; i++)
        {
            gate_means[i] = (float)gates[i];
            gate_vars[i] = 0.0f;
        }
        stats_initialized = true;
    }

    bool outlier_detected = false;
    for (int i = 0; i < 16; i++)
    {
        float g = (float)gates[i];

        float old_mean = gate_means[i];
        float old_var = gate_vars[i];

        float new_mean = (decay_factor * g) + ((1.0f - decay_factor) * old_mean);

        float delta = g - old_mean;
        float new_var = (1.0f - decay_factor) * (old_var + decay_factor * delta * delta);

        gate_means[i] = new_mean;
        gate_vars[i] = new_var;

        float stdev = sqrtf(new_var);
        float upper_threshold = new_mean + (outlier_threshold * stdev);
        float lower_threshold = new_mean - (outlier_threshold * stdev);
        if (g > upper_threshold || g < lower_threshold)
        {
            outlier_detected = true;
        }
    }

    uint8_t presence = outlier_detected ? 1 : 0;
    update_hap_occupancy(presence);

    ESP_LOGD(TAG, "Presence: %u", presence);
    ESP_LOGD(TAG, "Distance: %u cm", distance_cm);
    ESP_LOGD(TAG, "Gates:\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u",
             gates[0], gates[1], gates[2], gates[3],
             gates[4], gates[5], gates[6], gates[7],
             gates[8], gates[9], gates[10], gates[11],
             gates[12], gates[13], gates[14], gates[15]);
}

void ld2420_read_task(void *param)
{
    static uint8_t data[256];
    static uint8_t local_buf[256 / 2];
    int pos = 0;

    while (1)
    {
        int len = uart_read_bytes(LD2420_UART, data, sizeof(data), pdMS_TO_TICKS(100));
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}
