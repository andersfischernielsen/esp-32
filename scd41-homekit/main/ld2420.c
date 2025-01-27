#include "ld2420.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ld2420";

bool ld2420_parse_simple_mode(const char *line, int len)
{
    uint16_t g_ld2420_distance = 0;
    bool g_ld2420_presence = false;

    if (strstr(line, "ON"))
    {
        g_ld2420_presence = true;
    }
    else if (strstr(line, "OFF"))
    {
        g_ld2420_presence = false;
    }

    const char *dist_ptr = strstr(line, "Range ");
    if (dist_ptr)
    {
        dist_ptr += 6;
        int distance = atoi(dist_ptr);
        if (distance >= 0 && distance <= 6000)
        {
            g_ld2420_distance = (uint16_t)distance;
        }
    }
    ESP_LOGI(TAG, "Saw %d cm", g_ld2420_distance);
    ESP_LOGI(TAG, "Saw %d", g_ld2420_presence);

    return g_ld2420_presence;
}