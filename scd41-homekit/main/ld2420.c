#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define LD2420_UART UART_NUM_2
#define LD2420_UART_BAUD_RATE 115200 // or 256000
#define LD2420_UART_RX_PIN (GPIO_NUM_16)
#define LD2420_UART_TX_PIN (GPIO_NUM_17)

#define LD2420_RX_BUFFER_SIZE 2048

static const uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static const uint32_t CMD_FRAME_FOOTER = 0x01020304;
// static const uint32_t ENERGY_FRAME_HEADER = 0xF1F2F3F4;
// static const uint32_t ENERGY_FRAME_FOOTER = 0xF5F6F7F8;
// static const uint32_t DEBUG_FRAME_HEADER = 0x1410BFAA;
// static const uint32_t DEBUG_FRAME_FOOTER = 0xFAFBFCFD;

static const uint16_t CMD_ENABLE_CONF = 0x00FF;  // Enter config mode
static const uint16_t CMD_DISABLE_CONF = 0x00FE; // Exit config mode
static const uint16_t CMD_READ_VERSION = 0x0000; // Get firmware version

#define LD2420_FRAME_MAX_LEN 64
#define CMD_FRAME_COMMAND 6
#define CMD_FRAME_DATA_LENGTH 4
#define CMD_ERROR_WORD 8

typedef enum
{
    LD2420_MODE_DEBUG = 0,
    LD2420_MODE_SIMPLE = 1,
    LD2420_MODE_ENERGY = 2,
} ld2420_operating_mode_t;

static ld2420_operating_mode_t g_current_mode = LD2420_MODE_SIMPLE;

static char g_ld2420_firmware_ver[32] = {0};

static void ld2420_uart_init(void)
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

static esp_err_t ld2420_send_command(uint16_t command, const uint8_t *data, size_t data_len)
{
    // Frame structure:
    // [Header(4 bytes)] [Length(2 bytes)] [Command(2 bytes)] [Data(optional)] [Footer(4 bytes)]
    uint8_t tx_buf[64];
    uint16_t frame_len = data_len + 2; // (2 bytes for command)
    uint16_t offset = 0;

    // Header
    memcpy(&tx_buf[offset], &CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER));
    offset += sizeof(CMD_FRAME_HEADER);

    // Length (data_len + 2)
    memcpy(&tx_buf[offset], &frame_len, sizeof(frame_len));
    offset += sizeof(frame_len);

    // Command (2 bytes)
    memcpy(&tx_buf[offset], &command, sizeof(command));
    offset += sizeof(command);

    // Optional data
    if (data_len > 0 && data != NULL)
    {
        memcpy(&tx_buf[offset], data, data_len);
        offset += data_len;
    }

    // Footer
    memcpy(&tx_buf[offset], &CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));
    offset += sizeof(CMD_FRAME_FOOTER);

    // Log TX in hex
    ESP_LOGI("LD2420", "TX Frame (%d bytes):", offset);
    ESP_LOG_BUFFER_HEX("LD2420", tx_buf, offset);

    // Send over UART
    int written = uart_write_bytes(LD2420_UART, (const char *)tx_buf, offset);
    if (written < 0)
    {
        ESP_LOGE("LD2420", "UART write error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// -----------------------------------------------------------------------
// Helper: enable or disable config mode
// -----------------------------------------------------------------------
static esp_err_t ld2420_set_config_mode(bool enable)
{
    uint16_t proto_ver = 0x0001;
    if (enable)
    {
        return ld2420_send_command(CMD_ENABLE_CONF,
                                   (const uint8_t *)&proto_ver,
                                   sizeof(proto_ver));
    }
    else
    {
        return ld2420_send_command(CMD_DISABLE_CONF, NULL, 0);
    }
}

// -----------------------------------------------------------------------
// Helper: read firmware version
// -----------------------------------------------------------------------
static esp_err_t ld2420_get_firmware_version(void)
{
    // Must be in config mode
    esp_err_t ret = ld2420_set_config_mode(true);
    if (ret != ESP_OK)
        return ret;

    // Send read version
    ret = ld2420_send_command(CMD_READ_VERSION, NULL, 0);

    // We'll parse the actual version in handle_ack_data() once it arrives
    vTaskDelay(pdMS_TO_TICKS(100));

    ld2420_set_config_mode(false);
    return ret;
}

static esp_err_t ld2420_auto_update_threshold()
{
    // The LD2410S expects the three 16-bit values in little-endian:
    //   Trigger factor = 0x0002
    //   Retention factor = 0x0001
    //   Scanning time = 0x0078
    // Each is 2 bytes, so total data_len = 6.

    uint16_t trigger = 0x0002;       // 2
    uint16_t retention = 0x0001;     // 1
    uint16_t scanning_time = 0x0078; // 120

    // Build a 6-byte payload in little-endian
    uint8_t data[6];
    memcpy(&data[0], &trigger, sizeof(trigger));
    memcpy(&data[2], &retention, sizeof(retention));
    memcpy(&data[4], &scanning_time, sizeof(scanning_time));

    // 0x0009 is the "Automatically update threshold" command
    return ld2420_send_command(0x0009, data, sizeof(data));
}

static esp_err_t ld2420_common_parameters()
{
    uint16_t param1 = 0x0005; // e.g. "max detection distance"
    uint16_t param2 = 0x000A; // e.g. "min detection distance"
    uint16_t param3 = 0x0006; // e.g. "no delay"
    uint16_t param4 = 0x0002; // e.g. "status frequency"
    uint16_t param5 = 0x000C; // e.g. "distance frequency"
    uint16_t param6 = 0x000B; // e.g. "response speed"

    uint8_t data[12];
    memcpy(&data[0], &param1, sizeof(param1));  // 2 bytes
    memcpy(&data[2], &param2, sizeof(param2));  // 2 bytes
    memcpy(&data[4], &param3, sizeof(param3));  // 2 bytes
    memcpy(&data[6], &param4, sizeof(param4));  // 2 bytes
    memcpy(&data[8], &param5, sizeof(param5));  // 2 bytes
    memcpy(&data[10], &param6, sizeof(param6)); // 2 bytes

    // Now data[] has 12 bytes total.
    return ld2420_send_command(0x0071, data, sizeof(data));
}

// -----------------------------------------------------------------------
// Minimal parse for command ACK frames
// -----------------------------------------------------------------------
static void handle_ack_data(const uint8_t *buffer, int length)
{
    // If length < ~12, it's not a complete ACK
    if (length < 12)
    {
        ESP_LOGW("LD2420", "ACK too short: %d bytes", length);
        return;
    }

    // The command = offset 6..7
    uint16_t cmd;
    memcpy(&cmd, &buffer[CMD_FRAME_COMMAND], sizeof(cmd));

    // The error code = offset 8..9
    uint16_t error_code;
    memcpy(&error_code, &buffer[CMD_ERROR_WORD], sizeof(error_code));
    if (error_code != 0)
    {
        ESP_LOGW("LD2420", "ACK Error 0x%04X for cmd=0x%04X", error_code, cmd);
        return;
    }

    // If it's the version command
    if (cmd == CMD_READ_VERSION)
    {
        // Typically offset 10 = length of the version string
        int str_size = buffer[10];
        if (str_size > 31)
            str_size = 31; // clamp
        memset(g_ld2420_firmware_ver, 0, sizeof(g_ld2420_firmware_ver));

        // Typically the version string starts at offset 12
        if (str_size > 0 && (12 + str_size) <= length)
        {
            memcpy(g_ld2420_firmware_ver, &buffer[12], str_size);
            ESP_LOGI("LD2420", "LD2420 Firmware Version: %s", g_ld2420_firmware_ver);
        }
        else
        {
            ESP_LOGI("LD2420", "LD2420 Firmware version parse mismatch. Full ACK:");
            ESP_LOG_BUFFER_HEX("LD2420", buffer, length);
        }
    }
}

// -----------------------------------------------------------------------
// Parse "Simple mode" line: e.g. "OFF Range 0\r\n" or "ON Range 123\r\n"
// -----------------------------------------------------------------------
static void handle_simple_mode_line(const uint8_t *buffer, int length)
{
    // Make a temp copy
    char temp[128] = {0};
    int cpy_len = (length < 127) ? length : 127;
    memcpy(temp, buffer, cpy_len);

    bool presence = false;
    if (strstr(temp, "ON"))
    {
        presence = true;
    }

    int distance_cm = 0;
    char *p = strstr(temp, "Range ");
    if (p)
    {
        distance_cm = atoi(p + 6);
    }
    ESP_LOGI("LD2420", "[Simple Mode] Presence: %s, Distance: %d cm",
             (presence ? "ON" : "OFF"), distance_cm);
}

// -----------------------------------------------------------------------
// Parse "Energy mode" frame
// -----------------------------------------------------------------------
static void handle_energy_frame(const uint8_t *buffer, int length)
{
    // Minimal check: typical size is ~45 bytes
    if (length < 45)
    {
        ESP_LOGW("LD2420", "Energy frame too short. Size=%d", length);
        return;
    }

    // presence at offset 6
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

    ESP_LOGI("LD2420", "[Energy Mode] Presence: %s, Distance: %u cm",
             (presence ? "Detected" : "None"), distance_cm);

    for (int i = 0; i < 16; i++)
    {
        ESP_LOGI("LD2420", "  Gate[%d] = %u", i, gates[i]);
    }
}

static void log_ascii_content(const uint8_t *data, int len)
{
    // This prints any 'printable' ASCII. Control characters get hex-escaped.
    char ascii[256];
    int idx = 0;

    for (int i = 0; i < len; i++)
    {
        if (data[i] >= 0x20 && data[i] < 0x7F)
        {
            // Printable ASCII range
            if (idx < (int)(sizeof(ascii) - 2))
            {
                ascii[idx++] = data[i];
            }
        }
        else
        {
            // For non-printables, insert something like "\xNN"
            if (idx < (int)(sizeof(ascii) - 5))
            {
                idx += snprintf(&ascii[idx], 5, "\\x%02X", data[i]);
            }
        }
    }

    ascii[idx] = '\0'; // null-terminate
    ESP_LOGI("LD2420", "RX: %s", ascii);
}

// -----------------------------------------------------------------------
// Main reading task: logs raw bytes and then does minimal frame assembly
// -----------------------------------------------------------------------
static void ld2420_read_task(void *param)
{
    static uint8_t data[LD2420_RX_BUFFER_SIZE];
    static uint8_t local_buf[2048];
    int pos = 0;

    while (1)
    {
        int len = uart_read_bytes(LD2420_UART, data, sizeof(data), pdMS_TO_TICKS(1000));
        if (len > 0)
        {
            log_ascii_content(data, len);
            ESP_LOGI("LD2420", "RX: %d bytes", len);
            ESP_LOG_BUFFER_HEX("LD2420", data, len);

            for (int i = 0; i < len; i++)
            {
                local_buf[pos++] = data[i];
                if (pos >= (int)sizeof(local_buf))
                {
                    // Overflow: reset
                    pos = 0;
                }

                // If we see CMD_FRAME_FOOTER (0x04 03 02 01), that might be an ACK
                if (pos >= 4)
                {
                    if (local_buf[pos - 4] == 0x04 &&
                        local_buf[pos - 3] == 0x03 &&
                        local_buf[pos - 2] == 0x02 &&
                        local_buf[pos - 1] == 0x01)
                    {
                        ESP_LOGI("LD2420", "ACK received");
                        handle_ack_data(local_buf, pos);
                        pos = 0;
                        continue;
                    }
                }

                // If we’re in Simple mode
                if (g_current_mode == LD2420_MODE_SIMPLE && pos >= 2)
                {
                    if (local_buf[pos - 2] == '\r' && local_buf[pos - 1] == '\n')
                    {
                        handle_simple_mode_line(local_buf, pos);
                        pos = 0;
                        continue;
                    }
                }

                // If we’re in Energy mode
                if (g_current_mode == LD2420_MODE_ENERGY && pos >= 4)
                {
                    if (local_buf[pos - 4] == 0xF5 &&
                        local_buf[pos - 3] == 0xF6 &&
                        local_buf[pos - 2] == 0xF7 &&
                        local_buf[pos - 1] == 0xF8)
                    {
                        handle_energy_frame(local_buf, pos);
                        pos = 0;
                        continue;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}