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
#ifndef UI_MAIN_C
#define UI_MAIN_C

#include <string.h>
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "ui_elements.h"
#include "ui_main.h"
#include "rotary_encoder.h"
#include "sd_card.h"
#include "sd_play.h"
#include "ff_handling.h"
#include "esp_vfs.h"
#include "saves.h"
#include "screens.h"
#include "format_helper.h"
#include "battery.h"
#include "soc/rtc.h"
#include "esp32s3/rom/rtc.h"
#include <sys/stat.h>

int64_t UI_MAIN_volume=5000;
#define UI_MAIN_LOG_TAG "UI_MAIN"

#define UI_MAIN_KEY_NOOP 0
#define UI_MAIN_KEY_MINUS 1
#define UI_MAIN_KEY_PLUS 2
#define UI_MAIN_KEY_SHORT_CLICK 3
#define UI_MAIN_KEY_LONG_CLICK 4
#define UI_MAIN_KEY_DOUBLE_CLICK 5
#define UI_MAIN_KEY_SHORT_CLICK_DELAYED_STAGE 6

typedef struct {
    uint8_t keyEvent;
} UI_MAIN_keyMessages_t;

uint64_t UI_MAIN_offTimestamp=0;
rotary_encoder_info_t UI_MAIN_rotaryCfg = { 0 };
SD_PLAY_message_t UI_MAIN_sdPlayMsgRecv;
SD_PLAY_message_t UI_MAIN_sdPlayMsgSend;
QueueHandle_t UI_MAIN_keyQueue=NULL;
QueueHandle_t UI_MAIN_scanQueueOut=NULL;
QueueHandle_t UI_MAIN_scanQueueIn=NULL;
QueueSetHandle_t UI_MAIN_keysAndPlay=NULL;
uint16_t UI_MAIN_amountOfBooks=0;
uint16_t UI_MAIN_amountOfFiles=0;
uint8_t reducedMode=1;
uint16_t UI_MAIN_sortedBookIDs[FF_MAX_FOLDER_ELEMENTS];
char UI_MAIN_selectedFolderName[FF_FILE_PATH_MAX];
char UI_MAIN_selectedFileName[FF_FILE_PATH_MAX];
uint16_t UI_MAIN_sortedFileIDs[FF_MAX_SORT_ELEMENTS];
QueueHandle_t UI_MAIN_rotaryEventQueue;

/**
  * @brief clocks the CPU to maximum speed
  *
  */
void UI_MAIN_cpuFull(){
    rtc_cpu_freq_config_t conf, cconf;
    rtc_clk_cpu_freq_get_config(&cconf);
    if(cconf.freq_mhz==240) return;
    if(!rtc_clk_cpu_freq_mhz_to_config(240, &conf)){
        ESP_LOGE(UI_MAIN_LOG_TAG,"CPU clock could not be set to %u MHz", 240);
    }else{
        ESP_LOGI(UI_MAIN_LOG_TAG,"CPU clock set to %u MHz", 240);
        rtc_clk_cpu_freq_set_config_fast(&conf);
    }
}

/**
  * @brief clocks the CPU to middle speed
  *
  */
void UI_MAIN_cpuMiddle(){
    rtc_cpu_freq_config_t conf, cconf;
    rtc_clk_cpu_freq_get_config(&cconf);
    if(cconf.freq_mhz==160) return;
    if(!rtc_clk_cpu_freq_mhz_to_config(160, &conf)){
        ESP_LOGE(UI_MAIN_LOG_TAG,"CPU clock could not be set to %u MHz", 160);
    }else{
        ESP_LOGI(UI_MAIN_LOG_TAG,"CPU clock set to %u MHz", 160);
        rtc_clk_cpu_freq_set_config_fast(&conf);
    }
}
/**
  * @brief clocks the CPU to normal speed
  *
  */
void UI_MAIN_cpuNormal(){
    rtc_cpu_freq_config_t conf, cconf;
    rtc_clk_cpu_freq_get_config(&cconf);
    if(cconf.freq_mhz==80) return;
    if(!rtc_clk_cpu_freq_mhz_to_config(80, &conf)){
        ESP_LOGE(UI_MAIN_LOG_TAG,"CPU clock could not be set to %u MHz", 80);
    }else{
        ESP_LOGI(UI_MAIN_LOG_TAG,"CPU clock set to %u MHz", 80);
        rtc_clk_cpu_freq_set_config_fast(&conf);
    }
}

SAVES_settings_t UI_MAIN_settings;
uint8_t UI_MAIN_doubleClick=0;
void UI_MAIN_rotaryTask(void *parm){
    rotary_encoder_set_queue(&UI_MAIN_rotaryCfg, UI_MAIN_rotaryEventQueue);
    rotary_encoder_event_t rotaryEvent = { 0 };
    uint64_t lastKeyPressedTime=esp_timer_get_time();
    UI_MAIN_keyMessages_t keyMessage;
    bool wasFading=false;
    while(1){
        memset(&keyMessage,0,sizeof(UI_MAIN_keyMessages_t));
        if (xQueueReceive(UI_MAIN_rotaryEventQueue, &rotaryEvent, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            uint64_t now=esp_timer_get_time();
            if(rotaryEvent.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE){
                keyMessage.keyEvent=UI_MAIN_KEY_PLUS;
                //ESP_LOGI(UI_MAIN_LOG_TAG, "KEY PLUS");
            }else if(rotaryEvent.state.direction == ROTARY_ENCODER_DIRECTION_COUNTER_CLOCKWISE){
                keyMessage.keyEvent=UI_MAIN_KEY_MINUS;
                //ESP_LOGI(UI_MAIN_LOG_TAG, "KEY MINUS");
            }else if(rotaryEvent.state.direction == ROTARY_ENCODER_SWITCH){
                //ESP_LOGI(UI_MAIN_LOG_TAG, "KEY SWITCH");
                if(rotaryEvent.state.switch_state==ROTARY_ENCODER_SWITCH_SHORT_CLICK){
                    if(reducedMode){
                        keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK;
                    }else{
                        keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK_DELAYED_STAGE;
                        if(UI_MAIN_doubleClick){
                            UI_MAIN_doubleClick=0;
                            keyMessage.keyEvent=UI_MAIN_KEY_DOUBLE_CLICK;
                            //ESP_LOGI(UI_MAIN_LOG_TAG,"DOUBLE CLICK");
                        }else{
                            UI_MAIN_doubleClick=1;
                        }
                    }
                }else if(rotaryEvent.state.switch_state==ROTARY_ENCODER_SWITCH_LONG_CLICK){
                    if(reducedMode){
                        keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK;
                    }else{
                        keyMessage.keyEvent=UI_MAIN_KEY_LONG_CLICK;
                        UI_MAIN_doubleClick=0;
                    }
                }
            }
            UI_MAIN_offTimestamp=now;
            lastKeyPressedTime=now;
            if((!UI_ELEMENTS_isDisplayOff())||(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK)){//send the key only, if display is on with double click exception
                //ESP_LOGI(UI_MAIN_LOG_TAG, "INQUEUE1: %i",keyMessage.keyEvent);
                if(xQueueSend(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(100))!=pdPASS){
                    ESP_LOGE(UI_MAIN_LOG_TAG,"Lost keystroke.");
                }
            }
            if(keyMessage.keyEvent!=UI_MAIN_KEY_SHORT_CLICK_DELAYED_STAGE){
                if(UI_ELEMENTS_isDisplayOff()||wasFading) UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness);
                wasFading=false;
                UI_ELEMENTS_displayOn();
            }
        }else{
            if(reducedMode){//in reduced mode the display stays on longer
                uint64_t now=esp_timer_get_time();
                if((now-lastKeyPressedTime)/1000>45000){
                    UI_ELEMENTS_displayOff();
                }
            }else{
                if(UI_MAIN_doubleClick>5){
                    keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK;
                    //ESP_LOGI(UI_MAIN_LOG_TAG,"SHORT CLICK");
                    if(!(UI_ELEMENTS_isDisplayOff()||wasFading)){//send the key only, if display is on and was not fading
                        //ESP_LOGI(UI_MAIN_LOG_TAG, "INQUEUE2: %i",keyMessage.keyEvent);
                        if(xQueueSend(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(100))!=pdPASS){
                            ESP_LOGE(UI_MAIN_LOG_TAG,"Lost keystroke.");
                        }
                    }
                    if(UI_ELEMENTS_isDisplayOff()||wasFading) UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness);
                    wasFading=false;
                    UI_ELEMENTS_displayOn();
                    UI_MAIN_doubleClick=0;
                }else if(UI_MAIN_doubleClick){
                    UI_MAIN_doubleClick++;
                }
                uint64_t now=esp_timer_get_time();
                if((now-lastKeyPressedTime)/1000>15000){
                    UI_ELEMENTS_displayOff();
                }else if((now-lastKeyPressedTime)/1000>12000){
                    wasFading=true;
                    uint64_t msUp=(((now-lastKeyPressedTime)/1000)-12000);//0....3000
                    uint64_t step=(msUp*UI_MAIN_settings.brightness)/3000;
                    uint64_t step2=(msUp*16*4)/3000;
                    //ESP_LOGI(UI_MAIN_LOG_TAG,"b: %d  s: %llu  m: %llu  s2: %llu",UI_MAIN_settings.brightness,step,msUp,step2);
                    if(UI_MAIN_settings.brightness-step>0){
                        UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness-step);
                    }else{
                        UI_ELEMENTS_setBrightness(1);
                    }
                    UI_ELEMENTS_disableChars(step2);
                }
            }
        }
    }
}

/**
  * @brief deinitializes the main player and most of the sub components for power off
  *
  */
void UI_MAIN_deInit(){
    // All done, unmount partition and disable SPIFFS
    SAVES_deInit();
    SD_CARD_deInit();
    ESP_LOGI(UI_MAIN_LOG_TAG, "SPIFFS unmounted");
}


char UI_MAIN_folderPath[FF_FILE_PATH_MAX];
char UI_MAIN_filePath[FF_FILE_PATH_MAX];

char* UI_MAIN_searchString=NULL;
int32_t* UI_MAIN_searchId=NULL;
char fileLine[255];
char UI_MAIN_dirEntry[255];
void UI_MAIN_scanSortTaskAllBooks(){
    UI_MAIN_cpuFull();
    memset(&UI_MAIN_sortedBookIDs[0],0,sizeof(UI_MAIN_sortedBookIDs));
    FF_getList(SD_CARD_MOUNT_POINT,&UI_MAIN_amountOfBooks,&UI_MAIN_sortedBookIDs[0],0,&UI_MAIN_scanQueueOut,&UI_MAIN_scanQueueIn,UI_MAIN_searchString,UI_MAIN_searchId);
    UI_MAIN_cpuNormal();
    vTaskDelete(NULL);
}

void UI_MAIN_scanSortTaskOneBook(){
    UI_MAIN_cpuFull();
    memset(&UI_MAIN_sortedFileIDs[0],0,sizeof(UI_MAIN_sortedFileIDs));
    FF_getList(&UI_MAIN_folderPath[0],&UI_MAIN_amountOfFiles,&UI_MAIN_sortedFileIDs[0],1,&UI_MAIN_scanQueueOut,&UI_MAIN_scanQueueIn,UI_MAIN_searchString,UI_MAIN_searchId);
    UI_MAIN_cpuNormal();
    vTaskDelete(NULL);
}

void UI_MAIN_screenOnIfNeeded(){
    rotary_encoder_event_t rotaryEvent;
    rotaryEvent.state.direction=ROTARY_ENCODER_DIRECTION_NOT_SET;
    rotaryEvent.state.switch_state=ROTARY_ENCODER_SWITCH_UNKOWN;
    xQueueSend(UI_MAIN_rotaryEventQueue,&rotaryEvent,100);//switch display on, if needed
}

uint32_t UI_MAIN_timeDiffNowS(uint64_t timeStamp){
    uint64_t now=esp_timer_get_time();
    if(timeStamp>now){
        return (timeStamp-now)/1000000;
    }
    return 0;
}

uint64_t UI_MAIN_wakeupTimer=0;
void UI_MAIN_setWakeupTimer(uint64_t timer){
    UI_MAIN_wakeupTimer=timer;
}

/**
  * @brief returns the setup wakeup timer in seconds for settings the RTC wake up time
  *
  * @return wake up time in seconds
  * 
  */
 uint64_t UI_MAIN_getWakeupTimer(){
    return UI_MAIN_wakeupTimer;
}

#define UI_MAIN_RUN_SM_INIT 0
#define UI_MAIN_RUN_SM_FOLDER_SCAN 5
#define UI_MAIN_RUN_SM_FOLDER_SELECTION 10
#define UI_MAIN_RUN_SM_SETUP_MENU 13
#define UI_MAIN_RUN_SM_BOOK_SCAN 15 
#define UI_MAIN_RUN_SM_PAUSED 20
#define UI_MAIN_RUN_SM_PLAY_INIT 24
#define UI_MAIN_RUN_SM_PLAYING 25

#define UI_MAIN_DEFAULT_SLEEP_TIME_S 300
SAVES_saveState_t UI_MAIN_saveState;
char UI_MAIN_pcWriteBuffer[2048];

/**
  * @brief the main player state machine that only exits if the player should shut down
  *
  * @param startUpFlags flags that explain the type of power up (e.g. watchdog or manual)
  * 
  */
 void UI_MAIN_run(uint8_t startUpFlags){
    uint8_t mainSM=UI_MAIN_RUN_SM_INIT;
    uint8_t pauseMode=0;
    uint16_t selectedFolder=0;
    uint16_t selectedFile=1;
    uint16_t oldSelectedFile=1;

    uint16_t currentPlayMinute=0;
    uint8_t currentPlaySecond=0;
    uint8_t currentFinishedFlag=0;
    uint16_t allPlayMinute=0;
    uint8_t allPlaySecond=0;
    uint8_t percent=0;
    uint64_t currentPlayPosition=0;
    uint64_t currentPlaySize=0;
    uint64_t currentPlayOffset=0;
    uint64_t currentPlayBlockSize=0;
    uint64_t currentPlayBitrate=0;
    int32_t currentPlayChannels=0;
    uint16_t currentPlaySpeed=100; //100=1.00, 300=3.00, 50=0.5
    uint8_t currentEqualizer=0;
    uint8_t currentRepeatMode=0;
    uint64_t lastSdPlayMessage=0;
    uint64_t lastPlayOverlayChangedTime=esp_timer_get_time()-5*1000000;
    uint64_t lastMinuteChangedTime=esp_timer_get_time()-5*1000000;
    uint8_t playOverlayMode=0;
    bool playOverlayActive=false;
    uint64_t sleepTimeOffTime=0;//the timestamp when the currently setup sleeptime is reached
    uint32_t sleepTimeSetupS=UI_MAIN_DEFAULT_SLEEP_TIME_S;//the setup sleeptime
    uint64_t reducedModeLastTimestamp=0;
    uint16_t newPlayMinute=0;
    uint64_t newPlayPosition=0;
    uint8_t saveFlag=0;
    uint64_t lastAutoSave=0;
    uint8_t extraFlags=0;
    UI_MAIN_keyMessages_t keyMessage;
    QueueHandle_t playerQueue=NULL;
    int32_t scanMessage=0;
    UI_MAIN_offTimestamp=esp_timer_get_time();

    int32_t searchId=-1;
    uint8_t powerOffSM=0;
    uint8_t sleepOffSM=0;
    uint8_t skipped=0;
    memset(&UI_MAIN_saveState,0,sizeof(SAVES_saveState_t));
    memset(&UI_MAIN_settings,0,sizeof(SAVES_settings_t));
    memset(&keyMessage,0,sizeof(UI_MAIN_keyMessages_t));
    xQueueReset(UI_MAIN_keyQueue);
    playerQueue=SD_PLAY_getMessageQueue();
    while(1){
        uint64_t now=esp_timer_get_time(); //make now available for all states of the state machine
        if((esp_timer_get_time()-UI_MAIN_offTimestamp)/1000000>60){//60s without playing or key strokes?->off
            return; // returning to main means deep sleep
        }
        if(esp_timer_get_time()/1000000>30){//30s after bootup check battery
            //if(esp_timer_get_time()/1000000>40) UI_MAIN_currentBattVoltage-=1000;
            if (BATTERY_getCurrentVoltage()<3350){
                switch(powerOffSM){
                    case 0: saveFlag=1;powerOffSM=1;
                            UI_MAIN_screenOnIfNeeded();
                            break;
                    case 1:
                            SCREENS_lowBattery(BATTERY_getCurrentVoltage());
                            UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                            SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                            for(uint16_t i=0;i<300;i++){//300*10ms=3s
                                SD_PLAY_getMessage(&UI_MAIN_sdPlayMsgRecv,1);//keep the queue empty while waiting
                                vTaskDelay(pdMS_TO_TICKS(10));
                            }
                            return;
                            break;
                }
            }
        }
        if((sleepTimeOffTime!=0)&&(sleepTimeOffTime<esp_timer_get_time())){
            switch(sleepOffSM){
                case 0: saveFlag=1;sleepOffSM=1;
                        break;
                case 1:
                        SCREENS_switchingOff(SD_CARD_getSize(),SAVES_getUsedSpaceLevel(),SAVES_cleanOldBookmarks(1));
                        UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                        SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                        for(uint16_t i=0;i<300;i++){//300*10ms=3s
                            SD_PLAY_getMessage(&UI_MAIN_sdPlayMsgRecv,1);//keep the queue empty while waiting
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                        return;
                        break;
            }
        }
        switch(mainSM){
            case UI_MAIN_RUN_SM_INIT:
                    if(SD_CARD_init()==0){
                        if(startUpFlags&UI_MAIN_STARTUPFLAG_IMAGE_PERSISTED) UI_MAIN_fwUpgradeRun(); //run upgrade checks only if persisted
                        mainSM=UI_MAIN_RUN_SM_FOLDER_SCAN;
                        UI_MAIN_searchId=NULL;
                        UI_MAIN_searchString=NULL;
                        if(SAVES_getUsedSpaceLevel()>=75){
                            SAVES_cleanOldBookmarks(0);
                        }
                        if(SAVES_loadSettings(&UI_MAIN_settings)==0){
                            reducedMode=UI_MAIN_settings.reducedMode;
                            if(UI_MAIN_settings.brightness!=0){
                                UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness);
                            }else{
                                UI_MAIN_settings.brightness=1;
                                UI_ELEMENTS_setBrightness(1);
                            }
                            UI_MAIN_volume=UI_MAIN_settings.volume;
                            if(UI_MAIN_settings.sleepTimeSetupS<60){
                                UI_MAIN_settings.sleepTimeSetupS=UI_MAIN_DEFAULT_SLEEP_TIME_S;
                            }
                            sleepTimeSetupS=UI_MAIN_settings.sleepTimeSetupS;
                            if(UI_MAIN_settings.lastFolderName[0]!=0){
                                UI_MAIN_searchString=&UI_MAIN_settings.lastFolderName[0];
                                UI_MAIN_searchId=&searchId;
                            }
                            if(UI_MAIN_settings.screenRotation){
                                UI_ELEMENTS_rotate(1);
                            }else{
                                UI_ELEMENTS_rotate(0);
                            }
                            if(UI_MAIN_settings.rotaryEncoderSpeed){
                                rotary_encoder_set_speed(1);
                            }else{
                                rotary_encoder_set_speed(0);
                            }
                            if(UI_MAIN_settings.rotaryEncoderDirection){
                                rotary_encoder_set_direction(1);
                            }else{
                                rotary_encoder_set_direction(0);
                            }
                        }
                        xTaskCreate(UI_MAIN_scanSortTaskAllBooks, "UI_MAIN_scanSortTaskAllBooks", 1024 * 5, NULL,  uxTaskPriorityGet(NULL), NULL);
                    }else{
                        SCREENS_noSdCard();
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        return; // returning to main means deep sleep
                    }
                    break;
            case UI_MAIN_RUN_SM_FOLDER_SCAN: 
                    if(xQueueReceive(UI_MAIN_scanQueueOut,&scanMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming scan messages
                        if(scanMessage==-1){//finished ok
                            if(UI_MAIN_amountOfBooks==0){//show empty folder symbol
                                SCREENS_noFolders();
                                vTaskDelay(pdMS_TO_TICKS(3000));
                                return; // returning to main means deep sleep
                            }else{
                                UI_MAIN_screenOnIfNeeded();
                                selectedFolder=1;
                                UI_MAIN_selectedFolderName[0]=0;
                                UI_MAIN_selectedFileName[0]=0;
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                extraFlags=0;
                                if(searchId>=0){//did we search for a folder that was last listened and found one?
                                    selectedFolder=searchId+1;
                                    strcpy(&UI_MAIN_selectedFolderName[0],&UI_MAIN_settings.lastFolderName[0]);

                                    keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK;//not very elegant jump right into the saved folder
                                    //xQueueReset(UI_MAIN_keyQueue);
                                    xQueueSend(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10));
                                }
                                UI_MAIN_searchString=NULL;
                                UI_MAIN_searchId=NULL;
                                searchId=-1;
                            }
                        }else if(scanMessage==-4){//scan canceled
                            mainSM=UI_MAIN_RUN_SM_INIT;
                        }else if(scanMessage>=0){
                            UI_MAIN_offTimestamp=esp_timer_get_time();
                            SCREENS_scanAllBooks(scanMessage);
                        }else{//some kind of error
                            mainSM=UI_MAIN_RUN_SM_INIT;
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_FOLDER_SELECTION:
                    if(selectedFolder==0){
                        UI_MAIN_cpuNormal();
                        SCREENS_setupSelect();
                    }else{
                        if(FF_getNameByID(SD_CARD_MOUNT_POINT,UI_MAIN_sortedBookIDs[selectedFolder-1],&UI_MAIN_selectedFolderName[0],0)!=0){
                            mainSM=UI_MAIN_RUN_SM_INIT;
                        }
                        if((extraFlags&0x80)==0){
                            UI_MAIN_folderPath[0]=0;
                            strcpy(&UI_MAIN_folderPath[0],SD_CARD_MOUNT_POINT"/");
                            strcat(&UI_MAIN_folderPath[0],&UI_MAIN_selectedFolderName[0]);
                            if(SAVES_existsBookmark(&UI_MAIN_selectedFolderName[0],&UI_MAIN_saveState)==0){
                                UI_MAIN_searchString=&UI_MAIN_saveState.fileName[0];
                                UI_MAIN_searchId=&searchId;
                                extraFlags|=0x81;
                                if(UI_MAIN_saveState.finished&0x01){
                                    extraFlags|=0x02;
                                }
                            }
                        }
                        SCREENS_folderSelect(selectedFolder,UI_MAIN_amountOfBooks,&UI_MAIN_selectedFolderName[0],UI_MAIN_getWakeupTimer(),extraFlags&0x7F);
                    }
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(50)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(selectedFolder>0){
                                selectedFolder--;
                            }else{
                                selectedFolder=UI_MAIN_amountOfBooks;
                            }
                            UI_ELEMENTS_textScrollyReset(2,0,12);
                            extraFlags=0;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(selectedFolder<UI_MAIN_amountOfBooks){
                                selectedFolder++;
                            }else{
                                selectedFolder=0;
                            }
                            UI_ELEMENTS_textScrollyReset(2,0,12);
                            extraFlags=0;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            if(selectedFolder==0){
                                mainSM=UI_MAIN_RUN_SM_SETUP_MENU;
                            }else{
                                xTaskCreate(UI_MAIN_scanSortTaskOneBook, "UI_MAIN_scanSortTaskOneBook", 1024 * 5, NULL,  uxTaskPriorityGet(NULL), NULL);
                                mainSM=UI_MAIN_RUN_SM_BOOK_SCAN;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            SCREENS_switchingOff(SD_CARD_getSize(),SAVES_getUsedSpaceLevel(),SAVES_cleanOldBookmarks(1));
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            return; // returning to main means deep sleep
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_SETUP_MENU:
                    if(UI_MAIN_setupMenu()!=0){
                        mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        extraFlags=0;
                    }
                    break;                    
            case UI_MAIN_RUN_SM_BOOK_SCAN:
                    if(xQueueReceive(UI_MAIN_scanQueueOut,&scanMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming scan messages
                        if(scanMessage==-1){//finished ok
                            if(UI_MAIN_amountOfFiles==0){//no files found, go back to folder selection
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                extraFlags=0;
                            }else{
                                pauseMode=0;//default chapter selection for a newly selected book
                                selectedFile=1;
                                oldSelectedFile=selectedFile;
                                currentPlayMinute=0;
                                currentPlaySecond=0;
                                allPlayMinute=0;
                                allPlaySecond=0;
                                currentPlaySize=0;
                                currentPlayOffset=0;
                                currentPlayBlockSize=0;
                                currentPlayChannels=0;
                                currentPlayBitrate=0;
                                newPlayMinute=0;
                                currentPlaySpeed=100;
                                currentEqualizer=0;
                                currentRepeatMode=0;
                                currentPlayPosition=0;
                                currentFinishedFlag=0;
                                mainSM=UI_MAIN_RUN_SM_PAUSED;
                                if(searchId>=0){//did we search for a filename that was last listened and found one?
                                    strcpy(&UI_MAIN_selectedFileName[0],&UI_MAIN_saveState.fileName[0]);
                                    selectedFile=searchId+1;
                                    oldSelectedFile=selectedFile;
                                    currentPlayMinute=UI_MAIN_saveState.playMinute;
                                    currentPlaySecond=UI_MAIN_saveState.playSecond;
                                    currentPlaySize=UI_MAIN_saveState.playSize;
                                    currentPlayOffset=UI_MAIN_saveState.playOffset;
                                    currentPlayBlockSize=UI_MAIN_saveState.playBlockSize;
                                    currentPlayBitrate=UI_MAIN_saveState.playBitrate;
                                    currentPlayChannels=UI_MAIN_saveState.playChannels;
                                    currentPlaySpeed=UI_MAIN_saveState.playSpeed;
                                    if(currentPlaySpeed<50) currentPlaySpeed=100;
                                    if(currentPlaySpeed>300) currentPlaySpeed=100;
                                    currentEqualizer=UI_MAIN_saveState.equalizer;
                                    currentRepeatMode=UI_MAIN_saveState.repeatMode;
                                    currentFinishedFlag=UI_MAIN_saveState.finished;
                                    currentPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,currentPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                                }else{//never played the the current track, get inital file information to be able to seek
                                    if(FF_getNameByID(&UI_MAIN_folderPath[0],UI_MAIN_sortedFileIDs[selectedFile-1],&UI_MAIN_selectedFileName[0],1)==0){
                                        strcpy(&UI_MAIN_filePath[0],&UI_MAIN_folderPath[0]);
                                        strcat(&UI_MAIN_filePath[0],"/");
                                        strcat(&UI_MAIN_filePath[0],&UI_MAIN_selectedFileName[0]);

                                        uint8_t fileFormat=FORMAT_HELPER_getFileType(&UI_MAIN_filePath[0]);
                                        struct stat st;
                                        if(stat(UI_MAIN_filePath, &st)==0){
                                            int32_t sampleRate=0,bits=0,channels=0;
                                            bool upmix;
                                            FILE *fileHandle=NULL;
                                            fileHandle = fopen(UI_MAIN_filePath,"r");
                                            
                                            if(fileFormat==FORMAT_HELPER_FILE_TYPE_AMR){
                                                FORMAT_HELPER_getAMRFormatInformation(fileHandle,&sampleRate,&bits,&channels,&currentPlayOffset,&currentPlayBitrate,&currentPlayBlockSize,st.st_size,&upmix);
                                            }else if(fileFormat==FORMAT_HELPER_FILE_TYPE_MP3){
                                                FORMAT_HELPER_getMP3FormatInformation(fileHandle,&sampleRate,&bits,&channels,&currentPlayOffset,&currentPlayBitrate,&currentPlayBlockSize,st.st_size);
                                            }else if(fileFormat==FORMAT_HELPER_FILE_TYPE_OGG){
                                                FORMAT_HELPER_getOGGFormatInformation(fileHandle,&sampleRate,&bits,&channels,&currentPlayOffset,&currentPlayBitrate,&currentPlayBlockSize,st.st_size);
                                            }
                                            currentPlaySize=st.st_size;
                                            currentPlayChannels=channels;
                                            if(fileHandle!=NULL){
                                                fclose(fileHandle);
                                            }
                                        }
                                    }
                                }
                                if((UI_MAIN_amountOfFiles==1)||(reducedMode)){
                                    reducedModeLastTimestamp=esp_timer_get_time();
                                    pauseMode=1;//no need for chapter selection if only one track exists, also reducedMode switches right to time seek mode
                                    newPlayMinute=currentPlayMinute;//but make sure the blinking adjustable position value is the same as the current position
                                }
                                SD_PLAY_setEqualizer(currentEqualizer);
                                UI_MAIN_screenOnIfNeeded();
                                UI_MAIN_searchString=NULL;
                                UI_MAIN_searchId=NULL;
                                searchId=-1;
                            }
                        }else if(scanMessage==-4){//scan canceled
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                            extraFlags=0;
                        }else if(scanMessage>=0){
                            UI_MAIN_offTimestamp=esp_timer_get_time();
                            SCREENS_scanOneBook(scanMessage);
                        }else{//some kind of error
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                            extraFlags=0;
                        }
                    }
                    if(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,0) == pdPASS ){//wait for incoming key messages
                        if((keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK)||(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK)){
                            scanMessage=-1;
                            xQueueOverwrite(UI_MAIN_scanQueueIn,&scanMessage);
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_PAUSED:
                    playOverlayMode=0;
                    UI_MAIN_cpuNormal();
                    if(pauseMode==0){//with chapter selection
                        FORMAT_HELPER_getPlayTimeByExtension(&currentPlaySecond,&currentPlayMinute,&percent,&UI_MAIN_selectedFileName[0],currentPlayPosition,currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,&allPlaySecond,&allPlayMinute,currentPlayChannels);
                        SCREENS_pause0(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],currentPlayMinute,currentPlaySecond,percent,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime));
                        if(reducedMode){
                            if((esp_timer_get_time()-reducedModeLastTimestamp)/1000>10000){//go back to book selection after 10s in chapter selection mode
                                reducedModeLastTimestamp=esp_timer_get_time();
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                extraFlags=0;
                            }
                        }
                    }else if(pauseMode==1){//with time selection
                        uint64_t dummy;
                        newPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,newPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                        FORMAT_HELPER_getPlayTimeByExtension((uint8_t*)&dummy,(uint16_t*)&dummy,&percent,&UI_MAIN_selectedFileName[0],newPlayPosition,currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,&allPlaySecond,&allPlayMinute,currentPlayChannels);
                        SCREENS_pause1(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],newPlayMinute,currentPlaySecond,percent,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime));
                        if(reducedMode){
                            if((esp_timer_get_time()-reducedModeLastTimestamp)/1000>10000){//switch to chapter selection or book selection after 10s in time selection mode
                                reducedModeLastTimestamp=esp_timer_get_time();
                                if(UI_MAIN_amountOfFiles==1){
                                    mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                    extraFlags=0;
                                }else{
                                    pauseMode=0;
                                }
                            }
                        }
                    }
                    if(startUpFlags&UI_MAIN_STARTUPFLAG_RTC){
                        startUpFlags&=~UI_MAIN_STARTUPFLAG_RTC;
                        mainSM=UI_MAIN_RUN_SM_PLAY_INIT;
                    }
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) reducedModeLastTimestamp=esp_timer_get_time();
                        if(pauseMode==0){//with chapter selection
                            if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                                if(selectedFile>1){
                                    selectedFile--;
                                }else{
                                    selectedFile=UI_MAIN_amountOfFiles;
                                }
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                                if(selectedFile<UI_MAIN_amountOfFiles){
                                    selectedFile++;
                                }else{
                                    selectedFile=1;
                                }
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                                mainSM=UI_MAIN_RUN_SM_PLAY_INIT;
                                if(oldSelectedFile!=selectedFile){
                                    currentPlayMinute=0;
                                    currentPlaySecond=0;
                                    currentPlayOffset=0;
                                    oldSelectedFile=selectedFile;
                                }
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                extraFlags=0;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            }
                        }else if(pauseMode==1){//with time selection
                            if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                                if(currentPlaySecond!=0){
                                    currentPlaySecond=0;
                                }else{
                                    if((now-lastMinuteChangedTime)>50000){
                                        if(newPlayMinute!=0){
                                            newPlayMinute--;
                                        }
                                    }else{
                                        if(newPlayMinute>=5){
                                            newPlayMinute-=5;
                                        }else{
                                            newPlayMinute=0;
                                        }
                                    }
                                }
                                lastMinuteChangedTime=now;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                                currentPlaySecond=0;
                                if((now-lastMinuteChangedTime)>50000){
                                    newPlayMinute++;
                                    newPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,newPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                                    if(newPlayPosition==0){
                                        if(newPlayMinute>0) newPlayMinute--;
                                    }
                                }else{
                                    newPlayMinute+=5;
                                    newPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,newPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                                    if(newPlayPosition==0){
                                        if(newPlayMinute>0) newPlayMinute-=5;
                                    }
                                }
                                lastMinuteChangedTime=now;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                                mainSM=UI_MAIN_RUN_SM_PLAY_INIT;
                                currentPlayMinute=newPlayMinute;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                                extraFlags=0;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            }
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_PLAY_INIT:
                    if(FF_getNameByID(&UI_MAIN_folderPath[0],UI_MAIN_sortedFileIDs[selectedFile-1],&UI_MAIN_selectedFileName[0],1)==0){
                        strcpy(&UI_MAIN_filePath[0],&UI_MAIN_folderPath[0]);
                        strcat(&UI_MAIN_filePath[0],"/");
                        strcat(&UI_MAIN_filePath[0],&UI_MAIN_selectedFileName[0]);
                        UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_START_PLAY;
                        UI_MAIN_sdPlayMsgSend.msgData=&UI_MAIN_filePath[0];
                        if((currentPlaySecond==0)&&(currentPlayMinute==0)) currentPlayOffset=0;
                        UI_MAIN_sdPlayMsgSend.filePos=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,currentPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                        SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume);
                        SD_PLAY_setPlaySpeed(currentPlaySpeed);
                        if(currentPlaySpeed>200){
                            UI_MAIN_cpuFull();
                        }else if(currentPlaySpeed>100){
                            UI_MAIN_cpuMiddle();
                        }else{
                            UI_MAIN_cpuNormal();
                        }
                        ESP_LOGI(UI_MAIN_LOG_TAG,"Trying to play %s at position %llu",UI_MAIN_sdPlayMsgSend.msgData,UI_MAIN_sdPlayMsgSend.filePos);
                        SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                        lastAutoSave=esp_timer_get_time();
                        lastSdPlayMessage=esp_timer_get_time();
                        allPlayMinute=0;
                        allPlaySecond=0;
                        mainSM=UI_MAIN_RUN_SM_PLAYING;
                    }else{//something went wrong
                        mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        extraFlags=0;
                    }
                    pauseMode=0;
                    saveFlag=0;
                    xQueueAddToSet(UI_MAIN_keyQueue,UI_MAIN_keysAndPlay);
                    xQueueAddToSet(playerQueue,UI_MAIN_keysAndPlay);
                    break;
            case UI_MAIN_RUN_SM_PLAYING:
                    if(!UI_ELEMENTS_isDisplayOff()){
                        if(((now-lastPlayOverlayChangedTime)/1000)<3000){
                            SCREENS_playOverlay(playOverlayMode,UI_MAIN_volume,currentPlaySpeed,currentEqualizer,currentRepeatMode,sleepTimeSetupS,sleepTimeOffTime);
                        }else{
                            playOverlayActive=false;
                            if(((now-lastPlayOverlayChangedTime)/1000)>10000){//wait longer for the overlay to switch back to volume setting first
                                playOverlayMode=0;
                            }
                            if(playOverlayMode==0){
                                SCREENS_play(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],currentPlayMinute,currentPlaySecond,percent,currentRepeatMode,allPlayMinute,allPlaySecond,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime),0);
                            }else{
                                SCREENS_play(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],currentPlayMinute,currentPlaySecond,percent,currentRepeatMode,allPlayMinute,allPlaySecond,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime),(now-lastPlayOverlayChangedTime)/1000);
                            }
                        }
                    }
                    QueueHandle_t rx=NULL;
                    do{
                        rx=(QueueHandle_t) xQueueSelectFromSet(UI_MAIN_keysAndPlay,pdMS_TO_TICKS(10));
                        now=esp_timer_get_time();
                        if((rx==playerQueue)&&(SD_PLAY_getMessage(&UI_MAIN_sdPlayMsgRecv,5)==0)){
                            UI_MAIN_offTimestamp=esp_timer_get_time();
                            lastSdPlayMessage=esp_timer_get_time();
                            if(UI_MAIN_sdPlayMsgRecv.msgType==SD_PLAY_MSG_TYPE_STOPPED_NO_ERROR){
                                ESP_LOGI(UI_MAIN_LOG_TAG,"OK Playpos: %llu",UI_MAIN_sdPlayMsgRecv.filePos);
                                if((UI_MAIN_sdPlayMsgRecv.filePos==UI_MAIN_sdPlayMsgRecv.fileSize)||(skipped)){//file finished
                                    skipped=0;
                                    currentPlayMinute=0;
                                    currentPlaySecond=0;
                                    currentPlayPosition=0;
                                    currentPlaySize=0;
                                    currentPlayOffset=0;
                                    currentPlayBlockSize=0;
                                    currentPlayBitrate=0;
                                    currentPlayChannels=0;
                                    selectedFile++;
                                    mainSM=UI_MAIN_RUN_SM_PLAY_INIT;
                                    if(selectedFile>UI_MAIN_amountOfFiles){//book finished, reset everything to start
                                        selectedFile=1;
                                        if(currentRepeatMode&UI_MAIN_REPEAT_FOLDER){
                                            mainSM=UI_MAIN_RUN_SM_PLAY_INIT;//go on if we should repeat
                                        }else{
                                            mainSM=UI_MAIN_RUN_SM_PAUSED;
                                            if(reducedMode) reducedModeLastTimestamp=esp_timer_get_time();
                                        }
                                    }
                                    oldSelectedFile=selectedFile;
                                }else{//user pressed pause
                                    mainSM=UI_MAIN_RUN_SM_PAUSED;
                                    if(reducedMode) reducedModeLastTimestamp=esp_timer_get_time();
                                    saveFlag=1;
                                    if(selectedFile==UI_MAIN_amountOfFiles){//listening to last track of book
                                        if(((uint64_t)100*(uint64_t)UI_MAIN_sdPlayMsgRecv.filePos/(uint64_t)UI_MAIN_sdPlayMsgRecv.fileSize)>95){
                                            currentFinishedFlag=1;
                                        }else{
                                            currentFinishedFlag=0;
                                        }
                                    }
                                }
                            }else if(UI_MAIN_sdPlayMsgRecv.msgType==SD_PLAY_MSG_TYPE_STOPPED_ERROR){
                                ESP_LOGI(UI_MAIN_LOG_TAG,"ERROR");
                                mainSM=UI_MAIN_RUN_SM_PAUSED;
                                if(reducedMode) reducedModeLastTimestamp=esp_timer_get_time();
                            }else if(UI_MAIN_sdPlayMsgRecv.msgType == SD_PLAY_MSG_TYPE_FILEPOS_STATE){
                                currentPlayPosition=UI_MAIN_sdPlayMsgRecv.filePos;
                                currentPlaySize=UI_MAIN_sdPlayMsgRecv.fileSize;
                                currentPlayOffset=UI_MAIN_sdPlayMsgRecv.offset;
                                currentPlayBlockSize=UI_MAIN_sdPlayMsgRecv.blockSize;
                                currentPlayBitrate=UI_MAIN_sdPlayMsgRecv.bitrate;
                                currentPlayChannels=UI_MAIN_sdPlayMsgRecv.channels;
                                //ESP_LOGI(UI_MAIN_LOG_TAG,"Size: %llu, Offset: %llu, BlockSize: %llu, bitrate: %llu",currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate);
                                FORMAT_HELPER_getPlayTimeByExtension(&currentPlaySecond,&currentPlayMinute,&percent,&UI_MAIN_selectedFileName[0],currentPlayPosition,currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,&allPlaySecond,&allPlayMinute,currentPlayChannels);
                                newPlayMinute=currentPlayMinute;
                                if(((esp_timer_get_time()-lastAutoSave)/1000000)>30){
                                    //saveFlag=1;
                                    vTaskGetRunTimeStats(&UI_MAIN_pcWriteBuffer[0]);ESP_LOGI(UI_MAIN_LOG_TAG,"\n%s",&UI_MAIN_pcWriteBuffer[0]);
                                    ESP_LOGI(UI_MAIN_LOG_TAG,"Heap free: %lu, mininmum free: %lu",esp_get_free_heap_size(),esp_get_minimum_free_heap_size());
                                    lastAutoSave=esp_timer_get_time();
                                }
                            }
                        }
                        if((rx==UI_MAIN_keyQueue)&&(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(5)) == pdPASS )){//wait for incoming key messages
                            if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                                if(playOverlayActive){//first turn should not already do some action
                                    switch(playOverlayMode){
                                        case 0:
                                                    if((now-lastPlayOverlayChangedTime)>50000){
                                                        if(UI_MAIN_volume>=0) UI_MAIN_volume-=5;
                                                    }else{
                                                        if(UI_MAIN_volume>=0) UI_MAIN_volume-=100;
                                                    }
                                                    if(UI_MAIN_volume<0) UI_MAIN_volume=0;
                                                    if(UI_MAIN_volume>10000) UI_MAIN_volume=10000;
                                                    SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume);
                                                    break;
                                        case 1:
                                                    if(sleepTimeSetupS>60){
                                                        if(sleepTimeOffTime!=0){
                                                            sleepTimeSetupS-=60;
                                                        }
                                                        sleepTimeOffTime=esp_timer_get_time()+(uint64_t)((uint64_t)sleepTimeSetupS*(uint64_t)1000000);
                                                    }else{
                                                        sleepTimeSetupS=0;
                                                        sleepTimeOffTime=0;
                                                    }
                                                    break;
                                        case 2:
                                                    if(currentPlaySpeed>50){
                                                        currentPlaySpeed-=5;
                                                    }
                                                    SD_PLAY_setPlaySpeed(currentPlaySpeed);
                                                    if(currentPlaySpeed>200){
                                                        UI_MAIN_cpuFull();
                                                    }else if(currentPlaySpeed>100){
                                                        UI_MAIN_cpuMiddle();
                                                    }else{
                                                        UI_MAIN_cpuNormal();
                                                    }
                                                    break;
                                        case 3:     if(currentEqualizer>0){
                                                        currentEqualizer--;
                                                    }
                                                    SD_PLAY_setEqualizer(currentEqualizer);
                                                    break;
                                        case 4:     if(currentRepeatMode&UI_MAIN_REPEAT_FOLDER){
                                                        currentRepeatMode&=~UI_MAIN_REPEAT_FOLDER;
                                                    }else{
                                                        currentRepeatMode|=UI_MAIN_REPEAT_FOLDER;
                                                    }
                                                    break;
                                    }
                                }
                                lastPlayOverlayChangedTime=now;playOverlayActive=true;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                                if(playOverlayActive){//first turn should not already do some action
                                    switch(playOverlayMode){
                                        case 0:
                                                    if((now-lastPlayOverlayChangedTime)>50000){
                                                        if(UI_MAIN_volume<=10000) UI_MAIN_volume+=5;
                                                    }else{
                                                        if(UI_MAIN_volume<=10000) UI_MAIN_volume+=100;
                                                    }
                                                    if(UI_MAIN_volume<0) UI_MAIN_volume=0;
                                                    if(UI_MAIN_volume>10000) UI_MAIN_volume=10000;
                                                    SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume);
                                                    break;
                                        case 1:
                                                    if(sleepTimeSetupS<480*60){
                                                        if(sleepTimeOffTime!=0){
                                                            sleepTimeSetupS+=60;
                                                        }
                                                        if(sleepTimeSetupS==0) sleepTimeSetupS=60;
                                                        sleepTimeOffTime=esp_timer_get_time()+(uint64_t)((uint64_t)sleepTimeSetupS*(uint64_t)1000000);
                                                    }else{
                                                        sleepTimeSetupS=480*60;
                                                        sleepTimeOffTime=esp_timer_get_time()+(uint64_t)((uint64_t)sleepTimeSetupS*(uint64_t)1000000);
                                                    }
                                                    break;
                                        case 2:
                                                    if(currentPlaySpeed<250){
                                                        currentPlaySpeed+=5;
                                                    }
                                                    SD_PLAY_setPlaySpeed(currentPlaySpeed);
                                                    if(currentPlaySpeed>200){
                                                        UI_MAIN_cpuFull();
                                                    }else if(currentPlaySpeed>100){
                                                        UI_MAIN_cpuMiddle();
                                                    }else{
                                                        UI_MAIN_cpuNormal();
                                                    }
                                                    break;
                                        case 3:     if(currentEqualizer<6){
                                                        currentEqualizer++;
                                                    }
                                                    SD_PLAY_setEqualizer(currentEqualizer);
                                                    break;
                                        case 4:     if(currentRepeatMode&UI_MAIN_REPEAT_FOLDER){
                                                        currentRepeatMode&=~UI_MAIN_REPEAT_FOLDER;
                                                    }else{
                                                        currentRepeatMode|=UI_MAIN_REPEAT_FOLDER;
                                                    }
                                                    break;
                                    }
                                }
                                lastPlayOverlayChangedTime=now;playOverlayActive=true;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                                if(playOverlayActive){
                                    if(playOverlayMode<4){
                                        playOverlayMode++;
                                    }else{
                                        playOverlayMode=0;
                                    }
                                    if(reducedMode) playOverlayMode=0;//stay in volume mode in reduced mode
                                    lastPlayOverlayChangedTime=now;playOverlayActive=true;
                                }else{
                                    UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                    SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                    pauseMode=1;
                                }
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                                UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                pauseMode=0;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                                if(!playOverlayActive){
                                    UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                    SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                    skipped=1;
                                }
                            }
                        }

                    }while(rx!=NULL);
                    if((UI_MAIN_timeDiffNowS(sleepTimeOffTime)>0)&&(UI_MAIN_timeDiffNowS(sleepTimeOffTime)<=10)){//volume fade out if sleep timer nears end
                        SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume-(UI_MAIN_volume/UI_MAIN_timeDiffNowS(sleepTimeOffTime)));
                    }
                    if((now-lastSdPlayMessage)/1000000>2){//2s no message from sd play
                        ESP_LOGE(UI_MAIN_LOG_TAG,"PLAYTIMEOUT");
                        mainSM=UI_MAIN_RUN_SM_PAUSED;
                        if(reducedMode) reducedModeLastTimestamp=esp_timer_get_time();
                    }
                    if(saveFlag){
                        ESP_LOGI(UI_MAIN_LOG_TAG,"Saving playposition.");
                        saveFlag=0;
                        if(currentPlaySecond>=5){
                            UI_MAIN_saveState.playMinute=currentPlayMinute;
                            UI_MAIN_saveState.playSecond=currentPlaySecond-5;
                        }else{
                            if(currentPlayMinute>0){
                                UI_MAIN_saveState.playMinute=currentPlayMinute-1;
                                UI_MAIN_saveState.playSecond=(60-(5-currentPlaySecond))%60;
                            }else{
                                UI_MAIN_saveState.playMinute=0;
                                UI_MAIN_saveState.playSecond=0;
                            }
                        }
                        UI_MAIN_saveState.playSize=currentPlaySize;
                        UI_MAIN_saveState.playPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(UI_MAIN_saveState.playSecond,UI_MAIN_saveState.playMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                        UI_MAIN_saveState.playOffset=currentPlayOffset;
                        UI_MAIN_saveState.playBlockSize=currentPlayBlockSize;
                        UI_MAIN_saveState.playBitrate=currentPlayBitrate;
                        UI_MAIN_saveState.playChannels=currentPlayChannels;
                        UI_MAIN_saveState.playSpeed=currentPlaySpeed;
                        UI_MAIN_saveState.equalizer=currentEqualizer;
                        UI_MAIN_saveState.repeatMode=currentRepeatMode;
                        UI_MAIN_saveState.finished=currentFinishedFlag;
                        strcpy(&UI_MAIN_saveState.fileName[0],&UI_MAIN_selectedFileName[0]);
                        SAVES_saveBookmark(&UI_MAIN_selectedFolderName[0],&UI_MAIN_saveState);
                        UI_MAIN_settings.volume=UI_MAIN_volume;
                        UI_MAIN_settings.sleepTimeSetupS=sleepTimeSetupS;
                        strcpy(&UI_MAIN_settings.lastFolderName[0],&UI_MAIN_selectedFolderName[0]);
                        SAVES_saveSettings(&UI_MAIN_settings);
                    }
                    if(mainSM!=UI_MAIN_RUN_SM_PLAYING){
                        xQueueRemoveFromSet(UI_MAIN_keyQueue,UI_MAIN_keysAndPlay);
                        xQueueRemoveFromSet(playerQueue,UI_MAIN_keysAndPlay);
                    }
                    break;
        }
    }
}

//do not want to use strto(u)l as it does return 0 on error which is a valid value for fw version
uint8_t UI_MAIN_hexStr2Byte(char* str, uint8_t* value){
    uint8_t v=0;
    for(uint8_t i=0;i<2;i++){
        v<<=4;
        if((str[i]>='0')&&(str[i]<='9')){
            v+=str[i]-'0';
        }else if((str[i]>='A')&&(str[i]<='F')){
            v+=str[i]-'A'+10;
        }else if((str[i]>='a')&&(str[i]<='f')){
            v+=str[i]-'a'+10;
        }else{
            return 0;
        }
    }
    *value=v;
    return 1;
}

void UI_MAIN_getFWupgradeFile(char* fwFileName,uint8_t* major,uint8_t* minor,uint8_t* patch){
    DIR *dir = opendir(FW_DIR_PATH);
    struct dirent *currentEntry;
    uint64_t combinedVersionNumber=0;
    uint64_t highestCombinedVersionNumber=0;
    if(dir){
        while ((currentEntry = readdir(dir)) != NULL) {
            uint16_t l=strlen(currentEntry->d_name);
            if((currentEntry->d_type!=DT_DIR)&&(l==15)){//only files are interesting fw file naming format: 01.02.03_ota.fw
                if(strncasecmp(&currentEntry->d_name[l-7],"_ota.fw",7)==0){
                    char majorStr[3];strncpy(&majorStr[0],&currentEntry->d_name[0],2);majorStr[2]=0;
                    char minorStr[3];strncpy(&minorStr[0],&currentEntry->d_name[3],2);minorStr[2]=0;
                    char patchStr[3];strncpy(&patchStr[0],&currentEntry->d_name[6],2);patchStr[2]=0;
                    uint8_t tmp=0;
                    if(UI_MAIN_hexStr2Byte(&majorStr[0],&tmp)){
                        combinedVersionNumber=(uint64_t)tmp*(uint64_t)256*(uint64_t)256;
                        if(UI_MAIN_hexStr2Byte(&minorStr[0],&tmp)){
                            combinedVersionNumber+=(uint64_t)tmp*(uint64_t)256;
                            if(UI_MAIN_hexStr2Byte(&patchStr[0],&tmp)){
                                combinedVersionNumber+=(uint64_t)tmp;
                            }else{
                                combinedVersionNumber=0;
                            }
                        }else{
                            combinedVersionNumber=0;
                        }
                    }else{
                        combinedVersionNumber=0;
                    }
                    if(combinedVersionNumber>highestCombinedVersionNumber){
                        highestCombinedVersionNumber=combinedVersionNumber;
                        *major= highestCombinedVersionNumber/((uint64_t)((uint64_t)256*(uint64_t)256));
                        *minor=(highestCombinedVersionNumber%( uint64_t)((uint64_t)256*(uint64_t)256))/(uint64_t)256;
                        *patch= highestCombinedVersionNumber%(uint64_t)256;
                        strcpy(fwFileName,&currentEntry->d_name[0]);
                    }
                }
            }
        }
    }
    if((*major==FW_MAJOR)&&(*minor==FW_MINOR)&&(*patch==FW_PATCH)){//ignore currently installed FW
        ESP_LOGI(UI_MAIN_LOG_TAG, "Newest firmware already installed.");
        fwFileName[0]=0;
        return;
    }
    if(highestCombinedVersionNumber!=0){
        ESP_LOGI(UI_MAIN_LOG_TAG, "New firmware detected: v%02X.%02X.%02X  file: %s",*major,*minor,*patch,fwFileName);
        return;
    }
    fwFileName[0]=0;
    ESP_LOGI(UI_MAIN_LOG_TAG, "No new firmware detected.");
}

typedef struct UI_MAIN_setupMenuDataStruct{
    uint8_t sm;
    uint8_t selectedSub;
    uint64_t lastMinuteChangedTime;
    uint64_t wakeupTimeSetupS;
    int32_t numberOfBookmarks;
}UI_MAIN_setupMenuData_t;
UI_MAIN_setupMenuData_t UI_MAIN_setupMenuData;
uint64_t setupMenuReducedModeLastTimestamp=0;
#define UI_MAIN_SETUP_MENU_SM_INIT 0
#define UI_MAIN_SETUP_MENU_SUB_SELECTION 5
#define UI_MAIN_SETUP_MENU_SM_WAKEUP_TIMER 10
#define UI_MAIN_SETUP_MENU_SM_SCREEN_SETUP 15
#define UI_MAIN_SETUP_MENU_SM_SCREEN_BRIGHTNESS 20
#define UI_MAIN_SETUP_MENU_SM_ROT_DIR 25
#define UI_MAIN_SETUP_MENU_SM_ROT_SPEED 30
#define UI_MAIN_SETUP_MENU_SM_BOOKMARK_DELETE 35
#define UI_MAIN_SETUP_MENU_SM_REDUCED_MODE 40
#define UI_MAIN_SETUP_MENU_FW_ACCEPT_MODE 45
#define UI_MAIN_SETUP_MENU_SM_CLEANUP 250

/**
  * @brief displays and handles all the setup menu settings
  * 
  * @return 0=setup still busy, >0= setup finished
  *
  */
uint8_t UI_MAIN_setupMenu(){
    uint64_t now=esp_timer_get_time(); //make now available for all states of the state machine
    UI_MAIN_keyMessages_t keyMessage;
    switch(UI_MAIN_setupMenuData.sm){
        case UI_MAIN_SETUP_MENU_SM_INIT:
                    setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                    UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                    UI_MAIN_setupMenuData.lastMinuteChangedTime=esp_timer_get_time()-5*1000000;
                    memset(&keyMessage,0,sizeof(UI_MAIN_keyMessages_t));
                    UI_MAIN_setupMenuData.numberOfBookmarks=SAVES_cleanOldBookmarks(1);
                    break;
        case UI_MAIN_SETUP_MENU_SUB_SELECTION:
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_setupMenuData.selectedSub==0){
                                UI_MAIN_setupMenuData.selectedSub=7;
                            }else{
                                UI_MAIN_setupMenuData.selectedSub--;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_setupMenuData.selectedSub==7){
                                UI_MAIN_setupMenuData.selectedSub=0;
                            }else{
                                UI_MAIN_setupMenuData.selectedSub++;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            switch(UI_MAIN_setupMenuData.selectedSub){
                                case 0: //wakeup timer
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_WAKEUP_TIMER;
                                        break;
                                case 1: //screen rotation
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_SCREEN_SETUP;
                                        break;
                                case 2: //screen rotation
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_SCREEN_BRIGHTNESS;
                                        break;
                                case 3: //rotary encoder direction
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_ROT_DIR;
                                        break;
                                case 4: //rotary encoder speed
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_ROT_SPEED;
                                        break;
                                case 5: //bookmark deletion
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_BOOKMARK_DELETE;
                                        break;
                                case 6: //reduced functionality mode
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_REDUCED_MODE;
                                        break;
                                case 7: //new fw acceptance mode
                                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_FW_ACCEPT_MODE;
                                        break;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_CLEANUP;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_CLEANUP;
                        }
                    }
                    switch(UI_MAIN_setupMenuData.selectedSub){
                        case 0: //wakeup timer
                                SCREENS_wakeupTimer(UI_MAIN_setupMenuData.wakeupTimeSetupS,0);
                                break;
                        case 1: //screen rotation
                                SCREENS_screenSetup(UI_MAIN_settings.screenRotation,0);
                                break;
                        case 2: //screen brightness
                                SCREENS_screenBrightnessSetup(UI_MAIN_settings.brightness-1,0);
                                break;
                        case 3: //rotary encoder direction
                                SCREENS_rotDirSetup(UI_MAIN_settings.rotaryEncoderDirection,0);
                                break;
                        case 4: //rotary encoder speed
                                SCREENS_rotSpeedSetup(UI_MAIN_settings.rotaryEncoderSpeed,0);
                                break;
                        case 5: //bookmark deletion
                                SCREENS_bookmarkDeletionSetup(UI_MAIN_setupMenuData.numberOfBookmarks,0);
                                break;
                        case 6: //reduced functionality mode
                                SCREENS_reducedModeSetup(UI_MAIN_settings.reducedMode,0);
                                break;
                        case 7: //new fw acceptance mode
                                SCREENS_fwAcceptSetup(UI_MAIN_isImagePersisted(),0);
                                break;
                    }
                    UI_ELEMENTS_numberSelect(0,0,UI_MAIN_setupMenuData.selectedSub+1,8,1);
                    UI_ELEMENTS_update();
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//save settings in reduced mode if nothing was selected after 10s
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_CLEANUP;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_WAKEUP_TIMER:
                    SCREENS_wakeupTimer(UI_MAIN_setupMenuData.wakeupTimeSetupS,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if((now-UI_MAIN_setupMenuData.lastMinuteChangedTime)>50000){
                                if(UI_MAIN_setupMenuData.wakeupTimeSetupS>60){
                                    UI_MAIN_setupMenuData.wakeupTimeSetupS-=60;
                                }else{
                                    UI_MAIN_setupMenuData.wakeupTimeSetupS=60;
                                }
                            }else{
                                if(UI_MAIN_setupMenuData.wakeupTimeSetupS>60*5){
                                    UI_MAIN_setupMenuData.wakeupTimeSetupS-=60*5;
                                }else{
                                    UI_MAIN_setupMenuData.wakeupTimeSetupS=60;
                                }
                            }
                            if(UI_MAIN_setupMenuData.wakeupTimeSetupS<60) UI_MAIN_setupMenuData.wakeupTimeSetupS=60;
                            UI_MAIN_setupMenuData.lastMinuteChangedTime=now;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if((now-UI_MAIN_setupMenuData.lastMinuteChangedTime)>50000){
                                UI_MAIN_setupMenuData.wakeupTimeSetupS+=60;
                            }else{
                                UI_MAIN_setupMenuData.wakeupTimeSetupS+=60*5;
                            }
                            if(UI_MAIN_setupMenuData.wakeupTimeSetupS>3600*24) UI_MAIN_setupMenuData.wakeupTimeSetupS=3600*24;
                            UI_MAIN_setupMenuData.lastMinuteChangedTime=now;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setWakeupTimer(UI_MAIN_setupMenuData.wakeupTimeSetupS);
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setWakeupTimer(0);
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setWakeupTimer(0);
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setWakeupTimer(0);
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_SCREEN_SETUP:
                    SCREENS_screenSetup(UI_MAIN_settings.screenRotation,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_settings.screenRotation){
                                UI_MAIN_settings.screenRotation=0;
                            }else{
                                UI_MAIN_settings.screenRotation=1;
                            }
                            UI_ELEMENTS_rotate(UI_MAIN_settings.screenRotation);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_settings.screenRotation){
                                UI_MAIN_settings.screenRotation=0;
                            }else{
                                UI_MAIN_settings.screenRotation=1;
                            }
                            UI_ELEMENTS_rotate(UI_MAIN_settings.screenRotation);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_SCREEN_BRIGHTNESS:
                    SCREENS_screenBrightnessSetup(UI_MAIN_settings.brightness-1,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_settings.brightness>5){
                                UI_MAIN_settings.brightness-=5;
                            }else{
                                UI_MAIN_settings.brightness=1;
                            }
                            UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_settings.brightness<251){
                                UI_MAIN_settings.brightness+=5;
                            }else{
                                UI_MAIN_settings.brightness=251;
                            }
                            UI_ELEMENTS_setBrightness(UI_MAIN_settings.brightness);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_ROT_DIR:
                    SCREENS_rotDirSetup(UI_MAIN_settings.rotaryEncoderDirection,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_settings.rotaryEncoderDirection){
                                UI_MAIN_settings.rotaryEncoderDirection=0;
                            }else{
                                UI_MAIN_settings.rotaryEncoderDirection=1;
                            }
                            rotary_encoder_set_direction(UI_MAIN_settings.rotaryEncoderDirection);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_settings.rotaryEncoderDirection){
                                UI_MAIN_settings.rotaryEncoderDirection=0;
                            }else{
                                UI_MAIN_settings.rotaryEncoderDirection=1;
                            }
                            rotary_encoder_set_direction(UI_MAIN_settings.rotaryEncoderDirection);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_ROT_SPEED:
                    SCREENS_rotSpeedSetup(UI_MAIN_settings.rotaryEncoderSpeed,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_settings.rotaryEncoderSpeed){
                                UI_MAIN_settings.rotaryEncoderSpeed=0;
                            }else{
                                UI_MAIN_settings.rotaryEncoderSpeed=1;
                            }
                            rotary_encoder_set_speed(UI_MAIN_settings.rotaryEncoderSpeed);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_settings.rotaryEncoderSpeed){
                                UI_MAIN_settings.rotaryEncoderSpeed=0;
                            }else{
                                UI_MAIN_settings.rotaryEncoderSpeed=1;
                            }
                            rotary_encoder_set_speed(UI_MAIN_settings.rotaryEncoderSpeed);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_BOOKMARK_DELETE:
                    SCREENS_bookmarkDeletionSetup(UI_MAIN_setupMenuData.numberOfBookmarks,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            SAVES_cleanOldBookmarks(15);
                            UI_MAIN_setupMenuData.numberOfBookmarks=SAVES_cleanOldBookmarks(1);
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
        case UI_MAIN_SETUP_MENU_SM_CLEANUP:
                    UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SM_INIT;
                    SAVES_saveSettings(&UI_MAIN_settings);
                    reducedMode=UI_MAIN_settings.reducedMode;
                    return 1;
                    break;

        case UI_MAIN_SETUP_MENU_SM_REDUCED_MODE:
                    SCREENS_reducedModeSetup(UI_MAIN_settings.reducedMode,1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(UI_MAIN_settings.reducedMode==0){
                                UI_MAIN_settings.reducedMode=1;
                            }else{
                                UI_MAIN_settings.reducedMode=0;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(UI_MAIN_settings.reducedMode==1){
                                UI_MAIN_settings.reducedMode=0;
                            }else{
                                UI_MAIN_settings.reducedMode=1;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;

        case UI_MAIN_SETUP_MENU_FW_ACCEPT_MODE:
                    if(UI_MAIN_isImagePersisted()){
                        UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                    }else{
                        SCREENS_fwAcceptSetup(UI_MAIN_isImagePersisted(),1);
                        while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                            if(reducedMode) setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                                UI_MAIN_persistImage();
                                UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                                UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                                UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                            }
                        }
                    }
                    if(reducedMode){
                        if((esp_timer_get_time()-setupMenuReducedModeLastTimestamp)/1000>10000){//go back after 10s no input in reduced mode
                            setupMenuReducedModeLastTimestamp=esp_timer_get_time();
                            UI_MAIN_setupMenuData.sm=UI_MAIN_SETUP_MENU_SUB_SELECTION;
                        }
                    }
                    break;
    }
    return 0;
}

/**
  * @brief checks if the current running firmware image is persisted
  *
  * @return 0= image is not persisted, 1=image is persisted
  * 
  */
uint8_t UI_MAIN_isImagePersisted(void){
    esp_ota_img_states_t imgState;
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_ERROR_CHECK(esp_ota_get_state_partition(running, &imgState));
    if (imgState == ESP_OTA_IMG_PENDING_VERIFY)
    {
        return 0;
    }
    return 1;
}

/**
  * @brief persist a firmware image
  *
  */
void UI_MAIN_persistImage(){
    esp_ota_mark_app_valid_cancel_rollback();
}

uint8_t UI_MAIN_buffer[16*1024];//keep the stack small

/**
  * @brief runs the whole firmware image process as state machine
  *        returns only if no firmware upgrade was done and the player should
  *        start normally, otherwise resets the system after upgrade run
  *
  */
void UI_MAIN_fwUpgradeRun(){
    uint8_t fwSM=0;
    UI_MAIN_keyMessages_t keyMessage;
    uint8_t newMajor=0,newMinor=0,newPatch=0;
    char fwFileName[FF_FILE_PATH_MAX];
    char fullFWFileName[FF_FILE_PATH_MAX];
    uint64_t startTime=esp_timer_get_time();
    UI_MAIN_getFWupgradeFile(&fwFileName[0],&newMajor,&newMinor,&newPatch);
    if(fwFileName[0]==0){
        return;
    }
    if(UI_MAIN_isImagePersisted()==0){
        return;
    }
    while(1){
        uint64_t now=esp_timer_get_time(); //make now available for all states of the state machine
        switch(fwSM){
            case 0: SCREENS_fwUpgradeInit(newMajor,newMinor,newPatch,-1);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(20)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            fwSM++;
                            startTime=now;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            return;
                        }
                    }
                    if(((now-startTime)/1000000)>3){
                        return;
                    }
                    break;
            case 1: SCREENS_fwUpgradeInit(newMajor,newMinor,newPatch,10-((now-startTime)/1000000));
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(20)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            return;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            return;
                        }                    }
                    if(((now-startTime)/1000000)>=10){
                        startTime=now;
                        fwSM++;
                    }
                    break;
            case 2:
                    SCREENS_fwUpgradeRunning(0);
                    strcpy(&fullFWFileName[0],FW_DIR_PATH"/");strcat(&fullFWFileName[0],&fwFileName[0]);
                    FILE *file = fopen(&fullFWFileName[0], "rb");
                    if(file){
                        ESP_LOGI(UI_MAIN_LOG_TAG,"Opened fw file: %s",&fullFWFileName[0]);
                        esp_ota_handle_t update_handle = 0;
                        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
                        fseek(file, 0, SEEK_END);
                        size_t fwSize = ftell(file);
                        size_t fwRemainingSize = fwSize;
                        fseek(file, 0, SEEK_SET);
                        uint8_t aborted=0;
                        if(fwSize>1024){
                            if(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle)==ESP_OK){
                                while ((fwRemainingSize > 0)&&(aborted==0)) {
                                    size_t readBytes=fread(UI_MAIN_buffer, 1, sizeof(UI_MAIN_buffer), file);
                                    if(readBytes>0){
                                        if(esp_ota_write(update_handle, UI_MAIN_buffer, readBytes)!=ESP_OK){
                                            esp_ota_abort(update_handle);
                                            aborted=1;
                                        }
                                        fwRemainingSize-=readBytes;
                                        SCREENS_fwUpgradeRunning(((fwSize-fwRemainingSize)*100)/fwSize);
                                        while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(20)) == pdPASS ){}
                                    }else{
                                        if(fwRemainingSize!=0){
                                            esp_ota_abort(update_handle);
                                            aborted=1;
                                        }
                                    }
                                }
                                if(aborted==0){
                                    if (esp_ota_end(update_handle) == ESP_OK) {
                                        if (esp_ota_set_boot_partition(update_partition)== ESP_OK) {
                                            ESP_LOGI(UI_MAIN_LOG_TAG, "Rebooting");
                                            esp_restart();
                                        }else{
                                            ESP_LOGE(UI_MAIN_LOG_TAG, "esp_ota_set_boot_partition failed.");
                                            fwSM=100;
                                        }
                                    }else{
                                        ESP_LOGE(UI_MAIN_LOG_TAG, "esp_ota_end failed.");
                                        fwSM=100;
                                    }
                                }else{
                                    ESP_LOGE(UI_MAIN_LOG_TAG,"Something went wrong during flashing.");
                                    fwSM=100;
                                }
                            }else{
                                ESP_LOGE(UI_MAIN_LOG_TAG,"Could not begin OTA process.");
                                fwSM=100;
                            }
                        }else{
                            ESP_LOGE(UI_MAIN_LOG_TAG,"FW file size is smaller than 1k.");
                            fwSM=100;
                        }
                    }else{
                        ESP_LOGE(UI_MAIN_LOG_TAG,"Could not open fw file: %s",&fullFWFileName[0]);
                        fwSM=100;
                    }
                    
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(20)) == pdPASS ){}
                    UI_MAIN_screenOnIfNeeded();
                    if(((now-startTime)/1000000)>=100){
                        fwSM++;
                    }
                    break;
            case 100:   SCREENS_fwUpgradeRunning(-2);//-2 = error upgrading
                        vTaskDelay(pdMS_TO_TICKS(4000));
                        return;
                        break;
            default: return;
        }
    }
}

/**
  * @brief initializes the main player logic which in turn initalizes most sub components
  *
  */
void UI_MAIN_init(){
    UI_ELEMENTS_init();
    uint8_t imageFlags=0;
    if(UI_MAIN_isImagePersisted()){
        imageFlags|=UI_MAIN_STARTUPFLAG_IMAGE_PERSISTED;
    }
    SCREENS_startUp(imageFlags);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(rotary_encoder_init(&UI_MAIN_rotaryCfg,5,6,7));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&UI_MAIN_rotaryCfg, true));
    ESP_LOGI(UI_MAIN_LOG_TAG,"INIT with prio: %i of %i (0=idle)",uxTaskPriorityGet(NULL),configMAX_PRIORITIES - 1);
    UI_MAIN_rotaryEventQueue = rotary_encoder_create_queue();
    xTaskCreate(UI_MAIN_rotaryTask, "UI_MAIN_rotaryTask", 1024 * 5, NULL, uxTaskPriorityGet(NULL), NULL);
    BATTERY_init();
    UI_MAIN_keyQueue=xQueueCreate(50, sizeof(UI_MAIN_keyMessages_t));
    UI_MAIN_scanQueueOut=xQueueCreate(1, sizeof(int32_t));
    UI_MAIN_scanQueueIn=xQueueCreate(1, sizeof(int32_t));
    UI_MAIN_keysAndPlay = xQueueCreateSet(100);
    memset(&UI_MAIN_setupMenuData,0,sizeof(UI_MAIN_setupMenuData_t));
    UI_MAIN_setupMenuData.wakeupTimeSetupS=3600;
    vTaskDelay(pdMS_TO_TICKS(1000));//let the rotary encoder get first valid values and to let the user see the init screen
    uint8_t cnt=0;
    do{
        if(rotary_encoder_switch_state()){
            cnt++;
            vTaskDelay(pdMS_TO_TICKS(10));
            if(cnt>10){
                UI_ELEMENTS_cls();
                UI_ELEMENTS_mainSymbol(10);
                UI_ELEMENTS_progressBar((100*(200-cnt))/200);
                UI_ELEMENTS_update();
            }
        }else{
            cnt=0;
            break;
        }
    }while(cnt<200);//2s
    if(cnt!=0){
        UI_ELEMENTS_cls();
        UI_ELEMENTS_mainSymbol(11);
        UI_ELEMENTS_update();
        ESP_LOGW(UI_MAIN_LOG_TAG,"Resetting config.");
        SAVES_spiffsFormat();
    }
    SAVES_init();
}

#endif
