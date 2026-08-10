#include "esp_private/esp_clk.h"
#include "hal/gpio_ll.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "bsp";

bool g_usb_charging_mode = false;
bool g_persistent_charging_effect = false;
int g_current_effect = 0;
int g_wled_brightness = 50;
int g_wled_adjust_dir = -1;

void load_wled_brightness(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        int32_t val = 50;
        err = nvs_get_i32(my_handle, "wled_bright", &val);
        if (err == ESP_OK) {
            g_wled_brightness = val;
        }
        
        int32_t dir = -1;
        err = nvs_get_i32(my_handle, "wled_dir", &dir);
        if (err == ESP_OK) {
            g_wled_adjust_dir = dir;
        }
        nvs_close(my_handle);
    }
}

void save_wled_brightness(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_i32(my_handle, "wled_bright", g_wled_brightness);
        nvs_set_i32(my_handle, "wled_dir", g_wled_adjust_dir);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

void adjust_wled_brightness(void) {
    if (!g_wled_on) return;
    
    g_wled_brightness += g_wled_adjust_dir;
    if (g_wled_brightness > 50) {
        g_wled_brightness = 50;
    }
    if (g_wled_brightness < 1) {
        g_wled_brightness = 1;
    }
    
    uint32_t target_duty = (g_wled_brightness * 255) / 100;
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target_duty, 50);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}

void board_gpio_init(void)
{
    gpio_config_t io_conf = {0};

    const uint64_t output_pins = (1ULL << GPIO_BATM_SIG) |
                                (1ULL << GPIO_ON_SIG) |
                                (1ULL << GPIO_LED3528_POWER_SIG) |
                                (1ULL << GPIO_LED_BLUE_SIG) |
                                (1ULL << GPIO_LED3528_LEFT_SIG) |
                                (1ULL << GPIO_LED3528_RIGHT_SIG);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = output_pins;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_set_level(GPIO_BATM_SIG, 0);
    gpio_set_level(GPIO_ON_SIG, 0);
    gpio_set_level(GPIO_LED3528_POWER_SIG, 0);
    gpio_set_level(GPIO_LED_BLUE_SIG, 0);
    gpio_set_level(GPIO_LED3528_LEFT_SIG, 0);
    gpio_set_level(GPIO_LED3528_RIGHT_SIG, 0);

    const uint64_t input_pins_no_pull = (1ULL << GPIO_VBUS_SIG) ;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = input_pins_no_pull;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    const uint64_t key1_pins_pulldown = (1ULL << GPIO_KEY1_SIG);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = key1_pins_pulldown;
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    const uint64_t key2_pins_pullup = (1ULL << GPIO_KEY2_SIG);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = key2_pins_pullup;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void wled_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 100000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_WLED_POWER_SIG,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ledc_fade_func_install(0);
    
    load_wled_brightness();
}

void set_wled_state(bool on)
{
    g_wled_on = on;
    uint32_t target_duty = on ? ((g_wled_brightness * 255) / 100) : 0;
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target_duty, 1000);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}

inline void delay_cycles(uint32_t cycles)
{
    uint32_t start = esp_cpu_get_cycle_count();
    while ((uint32_t)(esp_cpu_get_cycle_count() - start) < cycles)
    {
        ;
    }
}

void set_power_state(power_state_t state)
{
    g_power_state = state;
    if (state == POWER_STATE_ON) {
        g_usb_charging_mode = false;
    }
    gpio_set_level(GPIO_ON_SIG, (state == POWER_STATE_ON) ? 1 : 0);
    gpio_set_level(GPIO_LED_BLUE_SIG, (state == POWER_STATE_ON) ? 1 : 0);
}

void set_usb_charging_mode(bool enable)
{
    g_usb_charging_mode = enable;
}

bool is_led3528_active(void)
{
    return (g_power_state == POWER_STATE_ON) || g_usb_charging_mode;
}

bool is_charging_effect_active(void)
{
    return g_usb_charging_mode || g_persistent_charging_effect;
}

void toggle_power_state(void)
{
    if (g_power_state == POWER_STATE_ON) {
        if (g_wled_on) {
            set_wled_state(false);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        set_power_state(POWER_STATE_OFF);
        g_persistent_charging_effect = false;
        set_usb_charging_mode(gpio_get_level(GPIO_VBUS_SIG) == 1);
    } else {
        g_persistent_charging_effect = false;
        set_usb_charging_mode(false);
        set_power_state(POWER_STATE_ON);
    }
    ESP_LOGI(TAG, "power_state=%s usb_charging_mode=%d",
             (g_power_state == POWER_STATE_ON) ? "ON" : "OFF",
             (int)g_usb_charging_mode);
}
