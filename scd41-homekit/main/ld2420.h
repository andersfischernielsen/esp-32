#pragma once

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

static const uint16_t CMD_ENABLE_CONF = 0x00FF;
static const uint16_t CMD_DISABLE_CONF = 0x00FE;
static const uint16_t CMD_READ_VERSION = 0x0000;

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

void ld2420_uart_init(void);
esp_err_t ld2420_send_command(uint16_t command, const uint8_t *data, size_t data_len);
esp_err_t ld2420_set_config_mode(bool enable);
esp_err_t ld2420_get_firmware_version(void);
void handle_ack_data(const uint8_t *buffer, int length);
void handle_simple_mode_line(const uint8_t *buffer, int length);
void handle_energy_frame(const uint8_t *buffer, int length);
void ld2420_read_task(void *param);
