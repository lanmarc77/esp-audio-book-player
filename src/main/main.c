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

//ffmpeg -i input.m4a -c:a copy -movflags +faststart output.m4a

/** Generating a working pre sorted folder index on root of sd card:
  * find * -type d -print |sort | iconv -t 437 > presorted.txt
  */ 
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "soc/rtc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"
#include "soc/rtc_cntl_reg.h"

#include "esp_sleep.h"
#include "driver/rtc_io.h"

#include "sd_play.h"
#include "ui_main.h"


#define MAIN_WATCHDOG_TIMEOUT 1000
#define MAIN_LOG_TAG "MAIN"

#define WAKEUP_PIN 7
void MAIN_deep_sleep(){
    gpio_set_level(36,1);//disable vext devices
    gpio_set_level(37,1);//disable ADC batt input

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);//leave pulldowns active even if in deep sleep
    ESP_LOGI(MAIN_LOG_TAG,"Pulldown Enable: %i",rtc_gpio_pulldown_en(WAKEUP_PIN));//enable pulldown on wakeup pin which is connected to button of rotary encoder
    ESP_LOGI(MAIN_LOG_TAG,"Wakupeset: %i",esp_sleep_enable_ext1_wakeup(1<<WAKEUP_PIN,ESP_EXT1_WAKEUP_ANY_HIGH)); //1 = High, 0 = Low, enable wakeup on ext1
    if(UI_MAIN_getWakeupTimer()>0){//should the device also wakeup by a setup wakeup timer?
        if(esp_sleep_enable_timer_wakeup(1000000ull*UI_MAIN_getWakeupTimer())==ESP_OK){
            ESP_LOGI(MAIN_LOG_TAG,"Timerwakeup set ok %llu",1000000ull*UI_MAIN_getWakeupTimer());
        }else{
            ESP_LOGE(MAIN_LOG_TAG,"Timerwakeup setup ERROR %llu",1000000ull*UI_MAIN_getWakeupTimer());
        }
    }
    vTaskDelay(pdTICKS_TO_MS(10));//leave some time for the pulldowns to stabilize the voltage
    esp_deep_sleep_start();
}
//displays stack sizes of freertos on terminal
void getStack(void)
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    unsigned long ulTotalRunTime;

   /* Take a snapshot of the number of tasks in case it changes while this
   function is executing. */
   uxArraySize = uxTaskGetNumberOfTasks();

   /* Allocate a TaskStatus_t structure for each task.  An array could be
   allocated statically at compile time. */
   pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

   if( pxTaskStatusArray != NULL )
   {
      /* Generate raw status information about each task. */
      uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
                                 uxArraySize,
                                 &ulTotalRunTime );
      for( x = 0; x < uxArraySize; x++ )
      {
         ESP_LOGI(MAIN_LOG_TAG, "%s %lu",pxTaskStatusArray[ x ].pcTaskName,pxTaskStatusArray[ x ].usStackHighWaterMark);
      }
      /* The array is no longer needed, free the memory it consumes. */
      vPortFree( pxTaskStatusArray );
   }
}



void app_main(void)
{
    uint8_t startUpFlags=0;
    esp_task_wdt_config_t wdtConfig;
    wdtConfig.timeout_ms=MAIN_WATCHDOG_TIMEOUT;
    wdtConfig.idle_core_mask=3;//monitor all 2 cores
    wdtConfig.trigger_panic=true;
    esp_task_wdt_reconfigure(&wdtConfig); //reconfigure watchdog with panic handler for both CPUs
    
    if(rtc_clk_slow_src_get()!=SOC_RTC_SLOW_CLK_SRC_XTAL32K){
        ESP_LOGW(MAIN_LOG_TAG,"Not using external 32khz crystal for timed wakeup.");
    }else{
        ESP_LOGI(MAIN_LOG_TAG,"Using external 32khz crystal for timed wakeup.");
    }

    //disable brownout as it might trigger for hungry devices on Vext pin, e.g. bigger SD cards
    uint32_t brownOutValue=READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
    brownOutValue&=~RTC_CNTL_BROWN_OUT_ENA;//disable
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brownOutValue);

    SD_PLAY_init();

    //switch Vext pin on
    esp_rom_gpio_pad_select_gpio(36);
    gpio_set_direction(36, GPIO_MODE_OUTPUT);
    gpio_set_level(36,0);//power Vext devices

    UI_MAIN_init();
    esp_wifi_stop(); //we do not need wifi at all
    SD_PLAY_startService();

    //display and react to type of wakeup
    esp_sleep_wakeup_cause_t wakeupCause=esp_sleep_get_wakeup_cause();
    ESP_LOGI(MAIN_LOG_TAG,"Wakeupcause: %i",wakeupCause);
    if(wakeupCause==ESP_SLEEP_WAKEUP_TIMER){
        ESP_LOGI(MAIN_LOG_TAG,"Wakeup by timer");
        startUpFlags|=UI_MAIN_STARTUPFLAG_RTC;
    }else if(wakeupCause==ESP_SLEEP_WAKEUP_EXT1){
        ESP_LOGI(MAIN_LOG_TAG,"Wakeup by button press");
        startUpFlags|=UI_MAIN_STARTUPFLAG_MANUAL;
    }

    UI_MAIN_run(startUpFlags);

    UI_MAIN_deInit();

    while(1){
        MAIN_deep_sleep();
    }
}
