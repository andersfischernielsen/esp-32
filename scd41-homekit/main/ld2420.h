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

#define LD2420_RX_BUFFER_SIZE 256

void ld2420_uart_init(void);
void ld2420_read_task(void *param);
void ld2420_send_enable_config(void);
void ld2420_send_report_mode(void);
