#include "ld2420.h"
#include "homekit.h"

void ld2420_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = LD2420_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(LD2420_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LD2420_UART,
                                 LD2420_UART_TX_PIN,
                                 LD2420_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(LD2420_UART, LD2420_RX_BUFFER_SIZE, 0, 0, NULL, 0));
}

esp_err_t ld2420_send_raw(const uint8_t *frame, size_t len)
{
    if (len > 64)
    {
        ESP_LOGE("LD2420", "Frame length %d exceeds limit of 64 bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOG_BUFFER_HEX("LD2420", frame, len);
    int written = uart_write_bytes(LD2420_UART, (const char *)frame, len);
    if (written < 0)
    {
        ESP_LOGE("LD2420", "UART write error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void ld2420_send_enable_config(void)
{
    uint8_t frame[] = {
        0xFD, 0xFC, 0xFB, 0xFA, // Header (FD FC FB FA)
        0x04, 0x00,             // Length (4 bytes)
        0xFF, 0x00,             // Command (0x00FF in little endian)
        0x02, 0x00,             // Protocol version (0x0002 in little endian)
        0x04, 0x03, 0x02, 0x01  // Footer (04 03 02 01)
    };

    ld2420_send_raw(frame, sizeof(frame));
}

void ld2420_send_report_mode(void)
{
    uint8_t frame[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x08, 0x00, 0x12, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
    ld2420_send_raw(frame, sizeof(frame));
}

void handle_energy_frame(const uint8_t *buffer, int length)
{
    if (length < 45)
    {
        ESP_LOGW("LD2420", "Energy frame too short. Size=%d", length);
        return;
    }

    uint8_t presence = buffer[6];
    uint16_t distance_cm = 0;
    memcpy(&distance_cm, &buffer[7], sizeof(distance_cm));

    uint16_t gates[16];
    int idx = 9;
    for (int i = 0; i < 16; i++)
    {
        memcpy(&gates[i], &buffer[idx], sizeof(uint16_t));
        idx += 2;
    }

    if (presence == 0x01)
    {
        update_hap_occupancy(1);
    }
    else
    {
        update_hap_occupancy(0);
    }

    ESP_LOGD("LD2420", "Presence: %u", presence);
    ESP_LOGD("LD2420", "Distance: %u cm", distance_cm);
    ESP_LOGD("LD2420", "Gates:\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u\n%u\t%u\t%u\t%u",
             gates[0], gates[1], gates[2], gates[3],
             gates[4], gates[5], gates[6], gates[7],
             gates[8], gates[9], gates[10], gates[11],
             gates[12], gates[13], gates[14], gates[15]);
}

void ld2420_read_task(void *param)
{
    static uint8_t data[LD2420_RX_BUFFER_SIZE];
    static uint8_t local_buf[LD2420_RX_BUFFER_SIZE / 2];
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

                handle_energy_frame(local_buf, frame_length);

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
