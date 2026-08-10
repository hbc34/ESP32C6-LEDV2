#include <stdbool.h>

#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "driver/gpio.h"

#include "DRV_3528.h"
#include "bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DRV_3528";

#define LED3528_BYTES_PER_SIDE (LED3528_LEDS_PER_SIDE * 3)
#define LED3528_TOTAL_BYTES (LED3528_TOTAL_LEDS * 3)

static void led3528_send_pixels_to_pin(gpio_num_t gpio_num, const uint8_t *data, size_t len)
{
    uint32_t cpu_freq = esp_clk_cpu_freq();
    uint32_t ticks_per_us = cpu_freq / 1000000;

    uint32_t t0h = ticks_per_us * 3 / 10;  // 0.3 us
    uint32_t t0l = ticks_per_us * 9 / 10;  // 0.9 us
    uint32_t t1h = ticks_per_us * 9 / 10;  // 0.9 us
    uint32_t t1l = ticks_per_us * 3 / 10;  // 0.3 us

    portDISABLE_INTERRUPTS();

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 7; b >= 0; b--) {
            bool bit = ((byte >> b) & 1) != 0;
            uint32_t high_ticks = bit ? t1h : t0h;
            uint32_t low_ticks = bit ? t1l : t0l;

            gpio_ll_set_level(&GPIO, gpio_num, 1);
            uint32_t start = esp_cpu_get_cycle_count();
            while ((uint32_t)(esp_cpu_get_cycle_count() - start) < high_ticks);

            gpio_ll_set_level(&GPIO, gpio_num, 0);
            start = esp_cpu_get_cycle_count();
            while ((uint32_t)(esp_cpu_get_cycle_count() - start) < low_ticks);
        }
    }

    portENABLE_INTERRUPTS();
}

void led3528_power_on(void)
{
    gpio_set_level(GPIO_LED3528_LEFT_SIG, 0);
    gpio_set_level(GPIO_LED3528_RIGHT_SIG, 0);
    gpio_set_level(GPIO_LED3528_POWER_SIG, 1);
    esp_rom_delay_us(100);
}

void led3528_power_off(void)
{
    gpio_set_level(GPIO_LED3528_LEFT_SIG, 0);
    gpio_set_level(GPIO_LED3528_RIGHT_SIG, 0);
    gpio_set_level(GPIO_LED3528_POWER_SIG, 0);
}

void hsv2rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}

void led3528_send_pixels(const uint8_t *data, size_t len)
{
    if (data == NULL) {
        return;
    }

    if (len != LED3528_TOTAL_BYTES) {
        ESP_LOGW(TAG, "expected %u bytes for %u LEDs, got %u",
                 (unsigned)LED3528_TOTAL_BYTES,
                 (unsigned)LED3528_TOTAL_LEDS,
                 (unsigned)len);
        if (len < LED3528_TOTAL_BYTES) {
            return;
        }
    }

    led3528_send_pixels_to_pin(GPIO_LED3528_LEFT_SIG, data, LED3528_BYTES_PER_SIDE);
    led3528_send_pixels_to_pin(GPIO_LED3528_RIGHT_SIG, data + LED3528_BYTES_PER_SIDE, LED3528_BYTES_PER_SIDE);
}
