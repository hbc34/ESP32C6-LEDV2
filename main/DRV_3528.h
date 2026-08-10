#ifndef __DRV_3528_H__
#define __DRV_3528_H__

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED3528_LEDS_PER_SIDE 40
#define LED3528_TOTAL_LEDS (LED3528_LEDS_PER_SIDE * 2)

void led3528_power_on(void);
void led3528_power_off(void);
void led3528_send_pixels(const uint8_t *data, size_t len);
void hsv2rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
