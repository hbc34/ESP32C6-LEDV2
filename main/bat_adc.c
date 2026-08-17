#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bat_adc";

adc_oneshot_unit_handle_t g_adc_handle;
adc_cali_handle_t g_adc_cali_handle;
bool g_adc_calibrated;
adc_unit_t g_bat_adc_unit;
adc_channel_t g_bat_adc_channel;
int g_battery_mv = 0;

void bat_adc_init(void)
{
    g_adc_calibrated = false;

    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(GPIO_BAT_ADC, &g_bat_adc_unit, &g_bat_adc_channel));

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = g_bat_adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &g_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(g_adc_handle, g_bat_adc_channel, &chan_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = g_bat_adc_unit,
        .chan = g_bat_adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &g_adc_cali_handle) == ESP_OK)
    {
        g_adc_calibrated = true;
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = g_bat_adc_unit,
        .chan = g_bat_adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &g_adc_cali_handle) == ESP_OK)
    {
        g_adc_calibrated = true;
    }
#endif

    ESP_LOGI(TAG, "bat_adc gpio=%d unit=%d channel=%d cali=%d", GPIO_BAT_ADC, g_bat_adc_unit, g_bat_adc_channel, (int)g_adc_calibrated);
}

void bat_adc_task(void *arg)
{
    while (1)
    {
        if (is_led3528_active())
        {
            bool blue_led_on_after_read = (g_power_state == POWER_STATE_ON);
            gpio_set_level(GPIO_LED_BLUE_SIG, 1);
            gpio_set_level(GPIO_BATM_SIG, 1);
            esp_rom_delay_us(2000);

            int raw = -1;
            esp_err_t err = adc_oneshot_read(g_adc_handle, g_bat_adc_channel, &raw);

            if (err == ESP_OK)
            {
                if (g_adc_calibrated)
                {
                    int mv = 0;
                    if (adc_cali_raw_to_voltage(g_adc_cali_handle, raw, &mv) == ESP_OK)
                    {
                        g_battery_mv = (int)(mv * 4.4f);
                        int pct = (g_battery_mv - 3000) * 100 / (4200 - 3000);
                        if (pct > 100) pct = 100;
                        if (pct < 0) pct = 0;
                        g_battery_percentage = pct;
                        ESP_LOGI(TAG, "BAT_ADC raw=%d mv=%d true_mv=%d pct=%d%%", raw, mv, g_battery_mv, pct);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "BAT_ADC raw=%d", raw);
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "BAT_ADC raw=%d", raw);
                }
            }
            else
            {
                ESP_LOGW(TAG, "BAT_ADC read failed: %s", esp_err_to_name(err));
            }
            gpio_set_level(GPIO_BATM_SIG, 0);
            gpio_set_level(GPIO_LED_BLUE_SIG, blue_led_on_after_read ? 1 : 0);

            for (int i = 0; i < 30; i++)
            {
                if (!is_led3528_active()) break;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
