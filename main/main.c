#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_cpu.h"
#include "bsp.h"
#include "RGB_EFFECT.h"
#include "bat_adc.h"
#include "key_action.h"
#include "nvs_flash.h"
static const char *TAG = "main";

power_state_t g_power_state = POWER_STATE_OFF;

bool g_key1_pressed;
bool g_key2_pressed;

volatile anim_cmd_t g_anim_cmd = ANIM_CMD_NONE;
int g_battery_percentage = 0;
bool g_wled_on = false;


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    board_gpio_init();
    wled_init();
    const bool vbus_present_at_boot = (gpio_get_level(GPIO_VBUS_SIG) == 1);
    const bool key1_pressed_at_boot = (gpio_get_level(GPIO_KEY1_SIG) == 1);

    if (key1_pressed_at_boot) {
        set_usb_charging_mode(false);
        set_power_state(POWER_STATE_ON);
    } else {
        set_power_state(POWER_STATE_OFF);
        set_usb_charging_mode(vbus_present_at_boot);
    }

    bat_adc_init();

    g_key1_pressed = key1_pressed_at_boot;
    g_key2_pressed = false;

    xTaskCreate(key1_check_task, "key1_check_task", 2048, NULL, 10, NULL);
    xTaskCreate(key2_check_task, "key2_check_task", 2048, NULL, 10, NULL);
    xTaskCreate(bat_adc_task, "bat_adc_task", 2048, NULL, 9, NULL);
    xTaskCreate(led3528_demo_task, "led3528_demo_task", 4096, NULL, 8, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
