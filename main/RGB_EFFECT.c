#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_random.h"
#include "DRV_3528.h"
#include "bsp.h"

#define NUM_LEDS LED3528_TOTAL_LEDS
#define SIDE_LEDS LED3528_LEDS_PER_SIDE

static const char *TAG = "RGB_EFFECT";

static void set_physical_pixel(uint8_t *data, int physical_idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (physical_idx < 0 || physical_idx >= NUM_LEDS) {
        return;
    }

    data[physical_idx * 3 + 0] = g;
    data[physical_idx * 3 + 1] = r;
    data[physical_idx * 3 + 2] = b;
}

static void set_left_pixel(uint8_t *data, int side_idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (side_idx < 0 || side_idx >= SIDE_LEDS) {
        return;
    }

    set_physical_pixel(data, side_idx, r, g, b);
}

static void set_right_pixel(uint8_t *data, int side_idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (side_idx < 0 || side_idx >= SIDE_LEDS) {
        return;
    }

    set_physical_pixel(data, SIDE_LEDS + side_idx, r, g, b);
}

static void set_both_side_pixel(uint8_t *data, int side_idx, uint8_t r, uint8_t g, uint8_t b)
{
    set_left_pixel(data, side_idx, r, g, b);
    set_right_pixel(data, side_idx, r, g, b);
}

// Circle order: right head -> right tail -> left tail -> left head.
static void set_circle_pixel(uint8_t *data, int circle_idx, uint8_t r, uint8_t g, uint8_t b)
{
    circle_idx %= NUM_LEDS;
    if (circle_idx < 0) {
        circle_idx += NUM_LEDS;
    }

    if (circle_idx < SIDE_LEDS) {
        set_right_pixel(data, circle_idx, r, g, b);
    } else {
        set_left_pixel(data, NUM_LEDS - 1 - circle_idx, r, g, b);
    }
}

void play_flash_effect(uint8_t r, uint8_t g, uint8_t b, int count)
{
    uint8_t led_data_on[NUM_LEDS * 3];
    uint8_t led_data_off[NUM_LEDS * 3] = {0};
    
    for (int i = 0; i < NUM_LEDS; i++) {
        led_data_on[i * 3 + 0] = g;
        led_data_on[i * 3 + 1] = r;
        led_data_on[i * 3 + 2] = b;
    }
    
    for (int c = 0; c < count; c++) {
        for (int i = 0; i < NUM_LEDS; i++) {
            int sum = led_data_on[i*3+0] + led_data_on[i*3+1] + led_data_on[i*3+2];
            if (sum > 255) {
                led_data_on[i*3+0] = (led_data_on[i*3+0] * 255) / sum;
                led_data_on[i*3+1] = (led_data_on[i*3+1] * 255) / sum;
                led_data_on[i*3+2] = (led_data_on[i*3+2] * 255) / sum;
            }
        }
        led3528_send_pixels(led_data_on, sizeof(led_data_on));
        esp_rom_delay_us(80);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        led3528_send_pixels(led_data_off, sizeof(led_data_off));
        esp_rom_delay_us(80);
        if (c < count - 1 || count == 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void play_battery_effect(void)
{
    int num_lit = (g_battery_percentage * SIDE_LEDS) / 100;
    if (num_lit > SIDE_LEDS) num_lit = SIDE_LEDS;
    
    uint8_t r = 0, g = 0, b = 0;
    if (g_battery_percentage <= 20) {
        r = 255; g = 0; b = 0; // Red
    } else if (g_battery_percentage <= 60) {
        r = 255; g = 255; b = 0; // Yellow
    } else {
        r = 0; g = 255; b = 0; // Green
    }
    
    uint8_t led_data[NUM_LEDS * 3] = {0};
    
    for (int i = 0; i < num_lit; i++) {
        set_both_side_pixel(led_data, i, r, g, b);
        for (int p = 0; p < NUM_LEDS; p++) {
            int sum = led_data[p*3+0] + led_data[p*3+1] + led_data[p*3+2];
            if (sum > 255) {
                led_data[p*3+0] = (led_data[p*3+0] * 255) / sum;
                led_data[p*3+1] = (led_data[p*3+1] * 255) / sum;
                led_data[p*3+2] = (led_data[p*3+2] * 255) / sum;
            }
        }
        led3528_send_pixels(led_data, sizeof(led_data));
        esp_rom_delay_us(80);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    
    bool is_charging = gpio_get_level(GPIO_VBUS_SIG) == 1;
    TickType_t start_wait = xTaskGetTickCount();
    
    if (is_charging && num_lit < SIDE_LEDS) {
        int breathe_idx = num_lit;
        while (xTaskGetTickCount() - start_wait < pdMS_TO_TICKS(2000)) {
            uint32_t elapsed = (xTaskGetTickCount() - start_wait) * portTICK_PERIOD_MS;
            int phase = elapsed % 1000;
            int brightness = phase < 500 ? (phase * 255 / 500) : ((1000 - phase) * 255 / 500);
            
            for (int i = 0; i < num_lit; i++) {
                set_both_side_pixel(led_data, i, r, g, b);
            }
            
            set_both_side_pixel(led_data, breathe_idx,
                                (r * brightness) / 255,
                                (g * brightness) / 255,
                                (b * brightness) / 255);

            if (is_charging) {
                int dot_pos = (elapsed / 30) % (num_lit + 1);
                if (dot_pos <= num_lit && dot_pos < SIDE_LEDS) {
                    set_both_side_pixel(led_data, dot_pos, 0, 0, 255);
                }
            }
            
            for (int p = 0; p < NUM_LEDS; p++) {
                int sum = led_data[p*3+0] + led_data[p*3+1] + led_data[p*3+2];
                if (sum > 255) {
                    led_data[p*3+0] = (led_data[p*3+0] * 255) / sum;
                    led_data[p*3+1] = (led_data[p*3+1] * 255) / sum;
                    led_data[p*3+2] = (led_data[p*3+2] * 255) / sum;
                }
            }
            led3528_send_pixels(led_data, sizeof(led_data));
            esp_rom_delay_us(80);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        set_both_side_pixel(led_data, breathe_idx, 0, 0, 0);
    } else {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    if (g_power_state != POWER_STATE_ON || is_charging_effect_active()) return;
    
    for (int i = 0; i < num_lit; i++) {
        set_both_side_pixel(led_data, i, 0, 0, 0);
        led3528_send_pixels(led_data, sizeof(led_data));
        esp_rom_delay_us(80);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void led3528_demo_task(void *arg)
{
    (void)arg;

    uint8_t hue = 0;
    uint32_t frame_count = 0;
    bool was_on = false;

    while (1)
    {
        if (is_led3528_active())
        {
            bool charging_effect = is_charging_effect_active();

            if (!was_on)
            {
                led3528_power_on();
                uint8_t off_data[NUM_LEDS * 3] = {0};
                led3528_send_pixels(off_data, sizeof(off_data));
                esp_rom_delay_us(80);
                was_on = true;

                if (!charging_effect) {
                    // smooth fade in
                    uint8_t fade_data[NUM_LEDS * 3] = {0};
                    for (int step = 0; step <= 20; step++) {
                        for (int i = 0; i < NUM_LEDS; i++) {
                            uint8_t r, g, b;
                            hsv2rgb((hue + i * 255 / NUM_LEDS) % 255, 255, 60, &r, &g, &b);
                            set_circle_pixel(fade_data, i,
                                             (r * step) / 20,
                                             (g * step) / 20,
                                             (b * step) / 20);

                            int sum = fade_data[i*3+0] + fade_data[i*3+1] + fade_data[i*3+2];
                            if (sum > 255) {
                                fade_data[i*3+0] = (fade_data[i*3+0] * 255) / sum;
                                fade_data[i*3+1] = (fade_data[i*3+1] * 255) / sum;
                                fade_data[i*3+2] = (fade_data[i*3+2] * 255) / sum;
                            }
                        }
                        led3528_send_pixels(fade_data, sizeof(fade_data));
                        esp_rom_delay_us(80);
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                }
            }

            if (g_power_state == POWER_STATE_ON && g_anim_cmd != ANIM_CMD_NONE) {
                anim_cmd_t cmd = g_anim_cmd;
                g_anim_cmd = ANIM_CMD_NONE;
                
                if (cmd == ANIM_CMD_FLASH_GREEN_THEN_BATTERY) {
                    play_flash_effect(0, 255, 0, 2);
                    cmd = ANIM_CMD_SHOW_BATTERY;
                } else if (cmd == ANIM_CMD_FLASH_BLUE_THEN_BATTERY) {
                    play_flash_effect(0, 0, 255, 2);
                    cmd = ANIM_CMD_SHOW_BATTERY;
                } else if (cmd == ANIM_CMD_SWITCH_EFFECT) {
                    uint8_t fade_data[NUM_LEDS * 3] = {0};
                    for (int step = 0; step <= 20; step++) {
                        for (int i = 0; i < NUM_LEDS; i++) {
                            fade_data[i * 3 + 0] = (60 * step) / 20; // G
                            fade_data[i * 3 + 1] = (60 * step) / 20; // R
                            fade_data[i * 3 + 2] = (60 * step) / 20; // B
                            
                            int sum = fade_data[i*3+0] + fade_data[i*3+1] + fade_data[i*3+2];
                            if (sum > 255) {
                                fade_data[i*3+0] = (fade_data[i*3+0] * 255) / sum;
                                fade_data[i*3+1] = (fade_data[i*3+1] * 255) / sum;
                                fade_data[i*3+2] = (fade_data[i*3+2] * 255) / sum;
                            }
                        }
                        led3528_send_pixels(fade_data, sizeof(fade_data));
                        esp_rom_delay_us(80);
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    for (int step = 20; step >= 0; step--) {
                        for (int i = 0; i < NUM_LEDS; i++) {
                            fade_data[i * 3 + 0] = (60 * step) / 20;
                            fade_data[i * 3 + 1] = (60 * step) / 20;
                            fade_data[i * 3 + 2] = (60 * step) / 20;
                            
                            int sum = fade_data[i*3+0] + fade_data[i*3+1] + fade_data[i*3+2];
                            if (sum > 255) {
                                fade_data[i*3+0] = (fade_data[i*3+0] * 255) / sum;
                                fade_data[i*3+1] = (fade_data[i*3+1] * 255) / sum;
                                fade_data[i*3+2] = (fade_data[i*3+2] * 255) / sum;
                            }
                        }
                        led3528_send_pixels(fade_data, sizeof(fade_data));
                        esp_rom_delay_us(80);
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    frame_count = 0;
                    hue = 0;
                }
                
                if (cmd == ANIM_CMD_SHOW_BATTERY) {
                    play_battery_effect();
                }
                
                g_anim_cmd = ANIM_CMD_NONE; 
            }

            uint8_t led_data[NUM_LEDS * 3] = {0};

            if (charging_effect) {
                int num_lit = (g_battery_percentage * SIDE_LEDS) / 100;
                if (num_lit > SIDE_LEDS) num_lit = SIDE_LEDS;
                uint8_t r = 0, g = 0, b = 0;
                if (g_battery_percentage <= 20) { r = 255; g = 0; b = 0; }
                else if (g_battery_percentage <= 60) { r = 255; g = 255; b = 0; }
                else { r = 0; g = 255; b = 0; }
                
                for (int i = 0; i < num_lit; i++) {
                    set_both_side_pixel(led_data, i, r, g, b);
                }
                
                uint32_t elapsed = xTaskGetTickCount() * portTICK_PERIOD_MS;
                bool is_charging = gpio_get_level(GPIO_VBUS_SIG) == 1;
                if (is_charging && num_lit < SIDE_LEDS) {
                    int phase = elapsed % 1000;
                    int brightness = phase < 500 ? (phase * 255 / 500) : ((1000 - phase) * 255 / 500);
                    set_both_side_pixel(led_data, num_lit,
                                        (r * brightness) / 255,
                                        (g * brightness) / 255,
                                        (b * brightness) / 255);
                }
                
                if (is_charging) {
                    int dot_pos = (elapsed / 30) % (num_lit + 1);
                    if (dot_pos <= num_lit && dot_pos < SIDE_LEDS) {
                        set_both_side_pixel(led_data, dot_pos, 0, 0, 255);
                    }
                }
            } else {
                if (g_current_effect == 0) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        uint8_t r, g, b;
                        uint8_t pixel_hue = hue + (i * 255 / NUM_LEDS);
                        hsv2rgb(pixel_hue, 255, 60, &r, &g, &b);
                        set_circle_pixel(led_data, i, r, g, b);
                    }
                    hue += 5;
                } else if (g_current_effect == 1) {
                    uint8_t r, g, b;
                    int phase = frame_count % 100;
                    int brightness = phase < 50 ? (phase * 60 / 50) : ((100 - phase) * 60 / 50);
                    if (phase == 0) hue += 50;
                    hsv2rgb(hue, 255, brightness, &r, &g, &b);
                    for (int i = 0; i < NUM_LEDS; i++) {
                        led_data[i * 3 + 0] = g;
                        led_data[i * 3 + 1] = r;
                        led_data[i * 3 + 2] = b;
                    }
                } else if (g_current_effect == 2) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        led_data[i*3+0]=0; led_data[i*3+1]=0; led_data[i*3+2]=0;
                    }
                    int pos = (frame_count / 2) % NUM_LEDS;
                    for (int i = 0; i < 5; i++) {
                        int p = (pos - i + NUM_LEDS) % NUM_LEDS;
                        int dim = 60 - i * 12;
                        if (dim < 0) dim = 0;
                        uint8_t dr, dg, db;
                        hsv2rgb(hue, 255, dim, &dr, &dg, &db);
                        set_circle_pixel(led_data, p, dr, dg, db);
                    }
                    hue += 1;
                } else if (g_current_effect == 3) {
                    int pos = (frame_count / 2) % NUM_LEDS;
                    if (pos == 0) hue += 30;
                    uint8_t r, g, b;
                    hsv2rgb(hue, 255, 60, &r, &g, &b);
                    for (int i = 0; i < NUM_LEDS; i++) {
                        if (i <= pos) {
                            set_circle_pixel(led_data, i, r, g, b);
                        } else {
                            uint8_t old_r, old_g, old_b;
                            hsv2rgb(hue - 30, 255, 60, &old_r, &old_g, &old_b);
                            set_circle_pixel(led_data, i, old_r, old_g, old_b);
                        }
                    }
                } else if (g_current_effect == 4) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        uint8_t r, g, b;
                        hsv2rgb(hue + i*10, 255, 20, &r, &g, &b);
                        set_circle_pixel(led_data, i, r, g, b);
                    }
                    if (frame_count % 3 == 0) {
                        int p = esp_random() % NUM_LEDS;
                        set_circle_pixel(led_data, p, 60, 60, 60);
                    }
                    hue += 2;
                }
            }

            for (int i = 0; i < NUM_LEDS; i++) {
                int sum = led_data[i*3+0] + led_data[i*3+1] + led_data[i*3+2];
                if (sum > 255) {
                    led_data[i*3+0] = (led_data[i*3+0] * 255) / sum;
                    led_data[i*3+1] = (led_data[i*3+1] * 255) / sum;
                    led_data[i*3+2] = (led_data[i*3+2] * 255) / sum;
                }
            }

            led3528_send_pixels(led_data, sizeof(led_data));
            esp_rom_delay_us(80);

            frame_count++;
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        else
        {
            if (was_on)
            {
                // smooth fade out
                uint8_t fade_data[NUM_LEDS * 3] = {0};
                for (int step = 20; step >= 0; step--) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        uint8_t r, g, b;
                        hsv2rgb((hue + i * 255 / NUM_LEDS) % 255, 255, 60, &r, &g, &b);
                        set_circle_pixel(fade_data, i,
                                         (r * step) / 20,
                                         (g * step) / 20,
                                         (b * step) / 20);
                        
                        int sum = fade_data[i*3+0] + fade_data[i*3+1] + fade_data[i*3+2];
                        if (sum > 255) {
                            fade_data[i*3+0] = (fade_data[i*3+0] * 255) / sum;
                            fade_data[i*3+1] = (fade_data[i*3+1] * 255) / sum;
                            fade_data[i*3+2] = (fade_data[i*3+2] * 255) / sum;
                        }
                    }
                    led3528_send_pixels(fade_data, sizeof(fade_data));
                    esp_rom_delay_us(80);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                
                uint8_t off_data[NUM_LEDS * 3] = {0};
                led3528_send_pixels(off_data, sizeof(off_data));
                
                esp_rom_delay_us(80);
                led3528_power_off();
                was_on = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
