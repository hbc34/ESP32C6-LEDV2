#ifndef __RGB_EFFECT_H__
#define __RGB_EFFECT_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void play_flash_effect(uint8_t r, uint8_t g, uint8_t b, int count);
void play_battery_effect(void);
void led3528_demo_task(void *arg);


#endif