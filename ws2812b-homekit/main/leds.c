#include "leds.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "neopixel.h"

#define TAG "ws2812"
#define PIXEL_COUNT 144
#define NEOPIXEL_PIN GPIO_NUM_8

static tNeopixelContext g_np_ctx = NULL;
static bool g_power = true;
static uint16_t g_hue = 0;
static uint8_t g_saturation = 0;
static uint8_t g_brightness = 70;
static tNeopixel g_pixels[PIXEL_COUNT];
static uint8_t g_base_r, g_base_g, g_base_b;
static uint8_t gamma_lut[256];

bool ws2812_get_power(void) { return g_power; }
int ws2812_get_brightness(void) { return g_brightness; }
float ws2812_get_hue(void) { return (float)g_hue; }
float ws2812_get_saturation(void) { return (float)g_saturation; }

void build_gamma_table(void)
{
    const float gamma = 2.8f;
    for (int i = 0; i < 256; ++i)
    {
        float norm = i / 255.0f;
        gamma_lut[i] = (uint8_t)(powf(norm, gamma) * 255.0f + 0.5f);
    }
}

void update_base_rgb(void)
{
    hsv2rgb(g_hue, g_saturation, 100,
            &g_base_r, &g_base_g, &g_base_b);
}

void update_pixels(void)
{
    if (!g_np_ctx)
        return;

    uint8_t scale = g_power
                        ? (uint16_t)g_brightness * 255 / 100
                        : 0;

    for (int i = 0; i < PIXEL_COUNT; ++i)
    {
        uint8_t r = (uint16_t)g_base_r * scale / 255;
        uint8_t gr = gamma_lut[r];
        uint8_t g = (uint16_t)g_base_g * scale / 255;
        uint8_t gg = gamma_lut[g];
        uint8_t b = (uint16_t)g_base_b * scale / 255;
        uint8_t gb = gamma_lut[b];

        g_pixels[i].index = i;
        g_pixels[i].rgb = NP_RGB(gr, gg, gb);
    }

    neopixel_SetPixel(g_np_ctx, g_pixels, PIXEL_COUNT);
}

void ws2812_init(void)
{
    g_np_ctx = neopixel_Init(PIXEL_COUNT, NEOPIXEL_PIN);
    if (!g_np_ctx)
    {
        ESP_LOGE(TAG, "neopixel_Init failed");
        return;
    }

    build_gamma_table();
    update_base_rgb();
    update_pixels();
}

void ws2812_deinit(void)
{
    neopixel_Deinit(g_np_ctx);
    g_np_ctx = NULL;
}

void ws2812_update_base_rgb(void)
{
    hsv2rgb(g_hue, g_saturation, 100,
            &g_base_r, &g_base_g, &g_base_b);
}

void hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
             uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    float hh = (h % 360) / 60.0f;
    int i = (int)hh;
    float f = hh - i;
    float vv = v / 100.0f;
    float ss = s / 100.0f;
    float p = vv * (1 - ss);
    float q = vv * (1 - ss * f);
    float t = vv * (1 - ss * (1 - f));
    float rf, gf, bf;

    switch (i)
    {
    case 0:
        rf = vv;
        gf = t;
        bf = p;
        break;
    case 1:
        rf = q;
        gf = vv;
        bf = p;
        break;
    case 2:
        rf = p;
        gf = vv;
        bf = t;
        break;
    case 3:
        rf = p;
        gf = q;
        bf = vv;
        break;
    case 4:
        rf = t;
        gf = p;
        bf = vv;
        break;
    default:
        rf = vv;
        gf = p;
        bf = q;
        break;
    }

    *out_r = (uint8_t)(rf * 255);
    *out_g = (uint8_t)(gf * 255);
    *out_b = (uint8_t)(bf * 255);
}

void ws2812_set_power(bool on)
{
    if (g_power == on) { return; }
    g_power = on;
    update_pixels();
}

void ws2812_set_brightness(int brightness)
{
    if (g_brightness == brightness) { return; }
    g_brightness = brightness;
    update_pixels();
}

void ws2812_set_hue(int hue)
{
    if (g_hue == hue) { return; }
    g_hue = hue % 360;
    update_base_rgb();
    update_pixels();
}

void ws2812_set_saturation(int saturation)
{
    if (g_saturation == saturation) { return; }
    g_saturation = saturation;
    update_base_rgb();
    update_pixels();
}
