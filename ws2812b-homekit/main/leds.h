#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void ws2812_init(void);
bool ws2812_get_power(void);
int ws2812_get_brightness(void);
float ws2812_get_hue(void);
float ws2812_get_saturation(void);
void ws2812_set_power(bool on);
void ws2812_set_brightness(int brightness);
void ws2812_set_hue(double hue);
void ws2812_set_saturation(double saturation);
