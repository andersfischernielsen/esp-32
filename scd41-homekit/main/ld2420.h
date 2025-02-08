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
#define LD2420_UART_BAUD_RATE 115200

esp_err_t ld2420_send_report_mode(void);
void ld2420_read_task(void *param);
