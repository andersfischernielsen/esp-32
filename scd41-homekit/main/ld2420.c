#include "ld2420.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "esp_log.h"

static bool g_ld2420_presence = false;

bool ld2420_parse_simple_mode(const char *line, int len)
{
    if (strstr(line, "ON"))
    {
        g_ld2420_presence = true;
    }
    else if (strstr(line, "OFF"))
    {
        g_ld2420_presence = false;
    }

    return g_ld2420_presence;
}