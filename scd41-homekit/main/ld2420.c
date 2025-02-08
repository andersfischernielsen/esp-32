#include "ld2420.h"
#include "homekit.h"
#include <math.h>

#define NO_OCCUPANCY_DELAY_MS 30000

static const char *TAG = "ld2420";

static const float DECAY_FACTOR = 0.0000000001f;
static const float MIN_STDEV = 1.7f;
static const float ABSOLUTE_MIN_CHANGE = 12.0f;

static float gate_offsets[16] = {0};
static float gate_scales[16] = {1};

static float gate_means[16] = {0};
static float gate_vars[16] = {0};
static bool stats_initialized = false;

static TickType_t last_detection_time = 0;

static TickType_t get_current_time_ticks(void)
{
    return xTaskGetTickCount();
}

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

static void print_gate_info(float raw_value, float normalized_value,
                            bool is_outlier,
                            float mean, float stdev,
                            float diff)
{
    const char *outlier_str = is_outlier ? "X" : "";

    ESP_LOGI(TAG,
             "RAW=%.1f,\tNORM=%.4f,\tMean=%.4f,\tStDev=%.4f,\tDiff=%.4f\t%s\t",
             raw_value, normalized_value,
             mean, stdev, diff, outlier_str);
}

void process_sensor_frame(const uint8_t *buffer, int length)
{
    if (length < 45)
    {
        ESP_LOGW(TAG, "Energy frame too short. Size=%d", length);
        return;
    }

    uint16_t distance_cm = 0;
    bool outlier_detected = false;
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
            float initial = (float)gates[i];
            gate_offsets[i] = initial;
            gate_scales[i] = (initial > 1.0f) ? initial : 1.0f;

            gate_means[i] = 0.0f;
            gate_vars[i] = 0.0f;
        }
        stats_initialized = true;
        last_detection_time = get_current_time_ticks();
    }

    for (int i = 0; i < 16; i++)
    {
        float raw_value = (float)gates[i];

        float g_norm = (raw_value - gate_offsets[i]) / gate_scales[i];

        float old_mean = gate_means[i];
        float old_var = gate_vars[i];

        float new_mean = (DECAY_FACTOR * g_norm) + ((1.0f - DECAY_FACTOR) * old_mean);

        float delta = g_norm - old_mean;
        float new_var = (1.0f - DECAY_FACTOR) * (old_var + DECAY_FACTOR * delta * delta);

        gate_means[i] = new_mean;
        gate_vars[i] = new_var;

        float stdev = sqrtf(new_var);
        if (stdev < MIN_STDEV)
        {
            stdev = MIN_STDEV;
        }

        float diff = fabsf(g_norm - new_mean);
        bool is_greater_than_min_change = diff >= (ABSOLUTE_MIN_CHANGE);

        if (is_greater_than_min_change)
        {
            outlier_detected = true;
        }
        // if (is_greater_than_min_change)
        // {
        //     print_gate_info(
        //         raw_value,
        //         g_norm,
        //         is_greater_than_min_change,
        //         new_mean,
        //         stdev,
        //         diff);
        // }
    }

    TickType_t now = get_current_time_ticks();
    uint8_t presence;
    if (outlier_detected)
    {
        last_detection_time = now;
        presence = 1;
    }
    else
    {
        if ((now - last_detection_time) >= pdMS_TO_TICKS(NO_OCCUPANCY_DELAY_MS))
        {
            presence = 0;
        }
        else
        {
            presence = 1;
        }
    }

    update_hap_occupancy(presence);

    ESP_LOGI(TAG, "Presence: %u", presence);
    ESP_LOGI(TAG, "Distance: %u cm", distance_cm);
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}
