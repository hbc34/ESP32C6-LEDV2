#ifndef __BSP_H__
#define __BSP_H__

#include <stdbool.h>

#include "soc/gpio_num.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define GPIO_BATM_SIG GPIO_NUM_0
#define GPIO_ON_SIG GPIO_NUM_1
#define GPIO_BAT_ADC GPIO_NUM_2
#define GPIO_VBUS_SIG GPIO_NUM_3
#define GPIO_LED3528_POWER_SIG GPIO_NUM_4
#define GPIO_KEY1_SIG GPIO_NUM_5
#define GPIO_KEY2_SIG GPIO_NUM_9
#define GPIO_WLED_POWER_SIG GPIO_NUM_7
#define GPIO_LED_BLUE_SIG GPIO_NUM_18
#define GPIO_LED3528_LEFT_SIG GPIO_NUM_19
#define GPIO_LED3528_RIGHT_SIG GPIO_NUM_20

typedef enum
{
    POWER_STATE_OFF = 0,
    POWER_STATE_ON = 1,
} power_state_t;

typedef enum {
    ANIM_CMD_NONE = 0,
    ANIM_CMD_SHOW_BATTERY,
    ANIM_CMD_FLASH_GREEN_THEN_BATTERY,
    ANIM_CMD_FLASH_BLUE_THEN_BATTERY,
    ANIM_CMD_SWITCH_EFFECT
} anim_cmd_t;

extern volatile anim_cmd_t g_anim_cmd;
extern int g_battery_percentage;
extern power_state_t g_power_state;
extern bool g_wled_on;
extern bool g_key1_pressed;
extern bool g_key2_pressed;
extern bool g_usb_charging_mode;
extern bool g_persistent_charging_effect;
extern int g_current_effect;
extern int g_wled_brightness;
extern int g_wled_adjust_dir;

void board_gpio_init(void);
void wled_init(void);
void set_wled_state(bool on);
void adjust_wled_brightness(void);
void save_wled_brightness(void);
void load_wled_brightness(void);
inline void delay_cycles(uint32_t cycles);
void set_power_state(power_state_t state);
void toggle_power_state(void);
void set_usb_charging_mode(bool enable);
bool is_led3528_active(void);
bool is_charging_effect_active(void);

#endif
