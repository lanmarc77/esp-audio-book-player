/*  This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef BATTERY_C
#define BATTERY_C
#include <freertos/FreeRTOS.h>
#include "battery.h"
#include "esp_log.h"
#include <freertos/queue.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"
#include "esp_rom_gpio.h"
#include "driver/gpio.h"

#define BATTERY_LOG_TAG "BAT"
uint32_t BATTERY_current=0;
uint32_t BATTERY_currentStable=0;
static int BATTERY_adc_raw[2][10];

static bool BATTERY_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(BATTERY_LOG_TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(BATTERY_LOG_TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(BATTERY_LOG_TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(BATTERY_LOG_TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(BATTERY_LOG_TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

void BATTERY_battVoltageTask(){
    esp_rom_gpio_pad_select_gpio(37);
    gpio_set_direction(37, GPIO_MODE_OUTPUT);
    gpio_set_level(37,0);//enable ADC batt input

    gpio_set_direction(2, GPIO_MODE_OUTPUT);
    gpio_set_level(2,0);//neighbour pin 2 to low to use as capacitor gnd input and reduce noise

    //calibration with factory defaults
    bool cali_enable = false;
    int voltage = 0;

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config));
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    cali_enable = BATTERY_adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_11, &adc1_cali_chan0_handle);

    while(1){
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &BATTERY_adc_raw[0][0]));
        //ESP_LOGI(BATTERY_LOG_TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_0, BATTERY_adc_raw[0][0]);
        if (cali_enable) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, BATTERY_adc_raw[0][0], &voltage));
            //ESP_LOGI(BATTERY_LOG_TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC_CHANNEL_0, voltage);
            voltage=((100+390)*voltage)/100;
            voltage+=150;
        }else{
            voltage=((100+390)*BATTERY_adc_raw[0][0])/100;
        }
        if(BATTERY_currentStable==0){
            BATTERY_currentStable=voltage;
        }else{
            BATTERY_currentStable=(voltage*1+BATTERY_currentStable*99)/100;//simple filter
        }
        if(BATTERY_current==0){
            BATTERY_current=voltage;
        }else{
            if(esp_timer_get_time()<10*1000*1000){//get the first 10s values faster
                BATTERY_current=(voltage*1+BATTERY_current*2)/3;//simple filter
            }else{
                BATTERY_current=(voltage*1+BATTERY_current*99)/100;//simple filter
            }
        }
        //ESP_LOGI(BATTERY_LOG_TAG, "filtered voltage: %lu mV", BATTERY_current);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
  * @brief   returns the current battery voltage in mV, the first 10s this value is
  *          using a very fast and responsive filter afterwords a slow but steady filter
  * 
  * @return  filtered battery voltage in mV
  */
uint32_t BATTERY_getCurrentVoltage(){
    return BATTERY_current;
}

/**
  * @brief   returns the current battery voltage in mV
  *          this function always uses the slow but steady filter
  * 
  * @return  filtered battery voltage in mV
  */
uint32_t BATTERY_getCurrentVoltageStable(){
    return BATTERY_currentStable;
}

/**
  * @brief    call once at program start to initalize the component
  */
void BATTERY_init(){
    xTaskCreate(BATTERY_battVoltageTask, "BATTERY_battVoltageTask", 1024 * 5, NULL, uxTaskPriorityGet(NULL), NULL);
}

#endif
