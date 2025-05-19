#include "leds.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "esp_log.h"
#include "led_strip.h"

#define TAG "ws2812"
#define PIXEL_COUNT 144
#define LED_GPIO_PIN GPIO_NUM_8

static bool g_power = true;
static double g_hue = 0;
static double g_saturation = 0.0f;
static double g_brightness = 70.0f;
static uint8_t g_base_r, g_base_g, g_base_b;
static uint8_t gamma_lut[256];

static led_strip_handle_t strip;

bool ws2812_get_power(void) { return g_power; }
int ws2812_get_brightness(void) { return g_brightness; }
float ws2812_get_hue(void) { return (float)g_hue; }
float ws2812_get_saturation(void) { return (float)g_saturation; }

void hsv2rgb(double h, double s, double v,
             uint8_t *out_r, uint8_t *out_g, uint8_t *out_b)
{
    double hh = (fmod(h, 360.0)) / 60.0;
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
    hsv2rgb(g_hue, g_saturation, 100, &g_base_r, &g_base_g, &g_base_b);
}

void update_pixels(void)
{
    uint8_t scale = g_power ? (uint16_t)g_brightness * 255 / 100 : 0;
    for (int i = 0; i < PIXEL_COUNT; ++i)
    {
        uint8_t r = (uint16_t)g_base_r * scale / 255;
        uint8_t g = (uint16_t)g_base_g * scale / 255;
        uint8_t b = (uint16_t)g_base_b * scale / 255;

        uint8_t r_corr = gamma_lut[r];
        uint8_t g_corr = gamma_lut[g];
        uint8_t b_corr = gamma_lut[b];

        led_strip_set_pixel(strip, i, r_corr, g_corr, b_corr);
    }
    led_strip_refresh(strip);
}

void ws2812_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,
        .max_leds = PIXEL_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }};

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    ESP_ERROR_CHECK(led_strip_clear(strip));

    build_gamma_table();
    update_base_rgb();
    update_pixels();
}

void ws2812_set_power(bool on)
{
    if (g_power == on)
        return;
    g_power = on;
    update_pixels();
}

void ws2812_set_brightness(int brightness)
{
    if (g_brightness == brightness)
        return;
    g_brightness = brightness;
    update_pixels();
}

void ws2812_set_hue(double hue)
{
    if (g_hue == hue)
        return;
    g_hue = fmod(hue, 360);
    update_base_rgb();
    update_pixels();
}

void ws2812_set_saturation(double saturation)
{
    if (g_saturation == saturation)
        return;
    g_saturation = saturation;
    update_base_rgb();
    update_pixels();
}
