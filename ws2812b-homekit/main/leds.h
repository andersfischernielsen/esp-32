#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void build_gamma_table(void);
void update_base_rgb(void);
void update_pixels(void);
void ws2812_init(void);
void ws2812_deinit(void);
void ws2812_update_base_rgb(void);
void ws2812_update_all_scaled(void);
void hsv2rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b);
void ws2812_set_power(bool on);
void ws2812_set_brightness(int brightness);
void ws2812_set_hue(int hue);
void ws2812_set_saturation(int saturation);
bool ws2812_get_power(void);
int ws2812_get_brightness(void);
float ws2812_get_hue(void);
float ws2812_get_saturation(void);