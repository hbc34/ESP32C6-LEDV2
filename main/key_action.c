#include "key_action.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "bsp.h"

static const char *TAG = "key_action";

void key1_check_task(void *arg)
{
    const TickType_t sample_ticks = pdMS_TO_TICKS(10);
    const TickType_t debounce_ticks = pdMS_TO_TICKS(50);
    const TickType_t longpress_ticks = pdMS_TO_TICKS(2000);

    int last_raw = gpio_get_level(GPIO_KEY1_SIG);
    int stable_level = last_raw;
    TickType_t stable_for = 0;

    bool pressed = (stable_level == 1);
    TickType_t press_start = xTaskGetTickCount();
    bool longpress_fired = pressed && (g_power_state == POWER_STATE_ON);
    TickType_t last_release_time = 0;

    int last_vbus_raw = gpio_get_level(GPIO_VBUS_SIG);
    int stable_vbus_level = last_vbus_raw;
    TickType_t stable_vbus_for = 0;

    while (1)
    {
        const int raw = gpio_get_level(GPIO_KEY1_SIG);
        if (raw == last_raw)
        {
            stable_for += sample_ticks;
        }
        else
        {
            last_raw = raw;
            stable_for = 0;
        }

        if (stable_for >= debounce_ticks && raw != stable_level)
        {
            stable_level = raw;
            if (stable_level == 1)
            {
                g_key1_pressed = true;
                pressed = true;
                press_start = xTaskGetTickCount();
                if (g_power_state == POWER_STATE_OFF) {
                    g_persistent_charging_effect = false;
                    set_usb_charging_mode(false);
                    set_power_state(POWER_STATE_ON);
                    longpress_fired = true;
                } else {
                    longpress_fired = false;
                }
            }
            else
            {
                g_key1_pressed = false;
                pressed = false;
                
                if (!longpress_fired && g_power_state == POWER_STATE_ON) {
                    TickType_t now = xTaskGetTickCount();
                    if (now - last_release_time < pdMS_TO_TICKS(400)) {
                        g_persistent_charging_effect = !g_persistent_charging_effect;
                        g_anim_cmd = ANIM_CMD_NONE; // Cancel single click animation if possible
                        last_release_time = 0;
                    } else {
                        g_anim_cmd = ANIM_CMD_SHOW_BATTERY;
                        last_release_time = now;
                    }
                }
            }
        }

        if (pressed && !longpress_fired)
        {
            const TickType_t now = xTaskGetTickCount();
            if ((now - press_start) >= longpress_ticks)
            {
                longpress_fired = true;
                toggle_power_state();
            }
        }

        const int vbus_raw = gpio_get_level(GPIO_VBUS_SIG);
        if (vbus_raw == last_vbus_raw) {
            stable_vbus_for += sample_ticks;
        } else {
            last_vbus_raw = vbus_raw;
            stable_vbus_for = 0;
        }

        if (stable_vbus_for >= debounce_ticks && vbus_raw != stable_vbus_level)
        {
            stable_vbus_level = vbus_raw;
            if (stable_vbus_level == 1) {
                if (g_power_state == POWER_STATE_ON) {
                    g_anim_cmd = ANIM_CMD_FLASH_GREEN_THEN_BATTERY;
                } else {
                    g_anim_cmd = ANIM_CMD_NONE;
                    set_usb_charging_mode(true);
                }
            } else {
                if (g_power_state == POWER_STATE_ON) {
                    g_anim_cmd = ANIM_CMD_FLASH_BLUE_THEN_BATTERY;
                    g_persistent_charging_effect = false;
                } else {
                    g_anim_cmd = ANIM_CMD_NONE;
                    g_persistent_charging_effect = false;
                    set_usb_charging_mode(false);
                }
            }
        }

        vTaskDelay(sample_ticks);
    }
}

void key2_check_task(void *arg)
{
    const TickType_t sample_ticks = pdMS_TO_TICKS(10);
    const TickType_t debounce_ticks = pdMS_TO_TICKS(50);
    const TickType_t longpress_ticks = pdMS_TO_TICKS(500);

    int last_raw = gpio_get_level(GPIO_KEY2_SIG);
    int stable_level = last_raw;
    TickType_t stable_for = 0;

    g_key2_pressed = (stable_level == 0);

    TickType_t last_release_time = 0;
    int click_count = 0;
    bool longpress_active = false;
    TickType_t press_start = 0;
    bool wait_for_release = false;

    while (1)
    {
        if (g_power_state != POWER_STATE_ON) {
            g_key2_pressed = (gpio_get_level(GPIO_KEY2_SIG) == 0);
            click_count = 0;
            longpress_active = false;
            wait_for_release = false;
            last_release_time = 0;
            vTaskDelay(sample_ticks);
            continue;
        }

        const int raw = gpio_get_level(GPIO_KEY2_SIG);
        if (raw == last_raw)
        {
            stable_for += sample_ticks;
        }
        else
        {
            last_raw = raw;
            stable_for = 0;
        }

        if (stable_for >= debounce_ticks && raw != stable_level)
        {
            stable_level = raw;
            bool pressed = (stable_level == 0);
            g_key2_pressed = pressed;
            
            if (pressed) {
                press_start = xTaskGetTickCount();
                longpress_active = false;
                wait_for_release = false;
            } else {
                if (!longpress_active && !wait_for_release) {
                    TickType_t now = xTaskGetTickCount();
                    if (now - last_release_time < pdMS_TO_TICKS(400)) {
                        click_count++;
                        if (click_count == 2) {
                            set_wled_state(!g_wled_on);
                            click_count = 0;
                            last_release_time = 0;
                        }
                    } else {
                        click_count = 1;
                        last_release_time = now;
                    }
                } else if (longpress_active) {
                    longpress_active = false;
                    g_wled_adjust_dir = -g_wled_adjust_dir;
                    save_wled_brightness();
                }
            }
        }

        if (!g_key2_pressed && click_count == 1) {
            if (xTaskGetTickCount() - last_release_time >= pdMS_TO_TICKS(400)) {
                g_current_effect = (g_current_effect + 1) % 5;
                g_anim_cmd = ANIM_CMD_SWITCH_EFFECT;
                click_count = 0;
            }
        }

        if (g_key2_pressed && !longpress_active && !wait_for_release) {
            if (xTaskGetTickCount() - press_start >= longpress_ticks) {
                longpress_active = true;
                click_count = 0;
            }
        }

        if (longpress_active && g_wled_on) {
            adjust_wled_brightness();
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            vTaskDelay(sample_ticks);
        }
    }
}
