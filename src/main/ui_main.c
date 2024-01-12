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
    if(!rtc_clk_cpu_freq_mhz_to_config(240, &conf)){
        ESP_LOGE(UI_MAIN_LOG_TAG,"CPU clock could not be set to %u MHz", 240);
    }else{
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
    if(!rtc_clk_cpu_freq_mhz_to_config(80, &conf)){
        ESP_LOGE(UI_MAIN_LOG_TAG,"CPU clock could not be set to %u MHz", 80);
    }else{
        rtc_clk_cpu_freq_set_config_fast(&conf);
    }
}

uint8_t UI_MAIN_doubleClick=0;
void UI_MAIN_rotaryTask(void *parm){
    rotary_encoder_set_queue(&UI_MAIN_rotaryCfg, UI_MAIN_rotaryEventQueue);
    rotary_encoder_event_t rotaryEvent = { 0 };
    uint64_t lastTime=esp_timer_get_time();
    UI_MAIN_keyMessages_t keyMessage;
    while(1){
        memset(&keyMessage,0,sizeof(UI_MAIN_keyMessages_t));
        if (xQueueReceive(UI_MAIN_rotaryEventQueue, &rotaryEvent, pdMS_TO_TICKS(500)) == pdTRUE)
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
                    keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK_DELAYED_STAGE;
                    if(UI_MAIN_doubleClick){
                        UI_MAIN_doubleClick=0;
                        keyMessage.keyEvent=UI_MAIN_KEY_DOUBLE_CLICK;
                        //ESP_LOGI(UI_MAIN_LOG_TAG,"DOUBLE CLICK");
                    }else{
                        UI_MAIN_doubleClick=1;
                    }
                }else if(rotaryEvent.state.switch_state==ROTARY_ENCODER_SWITCH_LONG_CLICK){
                    keyMessage.keyEvent=UI_MAIN_KEY_LONG_CLICK;
                    UI_MAIN_doubleClick=0;
                }
            }
            UI_MAIN_offTimestamp=now;
            lastTime=now;
            if((!UI_ELEMENTS_isDisplayOff())||(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK)){//send the key only, if display is on with double click exception
                //ESP_LOGI(UI_MAIN_LOG_TAG, "INQUEUE1: %i",keyMessage.keyEvent);
                if(xQueueSend(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(100))!=pdPASS){
                    ESP_LOGE(UI_MAIN_LOG_TAG,"Lost keystroke.");
                }
            }
            if(keyMessage.keyEvent!=UI_MAIN_KEY_SHORT_CLICK_DELAYED_STAGE){
                UI_ELEMENTS_displayOn();
            }
        }else{
            if(UI_MAIN_doubleClick){
                keyMessage.keyEvent=UI_MAIN_KEY_SHORT_CLICK;
                //ESP_LOGI(UI_MAIN_LOG_TAG,"SHORT CLICK");
                if(!UI_ELEMENTS_isDisplayOff()){//send the key only, if display is on
                    //ESP_LOGI(UI_MAIN_LOG_TAG, "INQUEUE2: %i",keyMessage.keyEvent);
                    if(xQueueSend(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(100))!=pdPASS){
                        ESP_LOGE(UI_MAIN_LOG_TAG,"Lost keystroke.");
                    }
                }
                UI_ELEMENTS_displayOn();
            }
            UI_MAIN_doubleClick=0;
            uint64_t now=esp_timer_get_time();
            if((now-lastTime)/1000000>30){
                UI_ELEMENTS_displayOff();
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
uint8_t* UI_MAIN_searchFlags=NULL;
char fileLine[255];
char UI_MAIN_dirEntry[255];
void UI_MAIN_scanSortTaskAllBooks(){
    UI_MAIN_cpuFull();
    memset(&UI_MAIN_sortedBookIDs[0],0,sizeof(UI_MAIN_sortedBookIDs));
    FF_getList(SD_CARD_MOUNT_POINT,&UI_MAIN_amountOfBooks,&UI_MAIN_sortedBookIDs[0],0,&UI_MAIN_scanQueueOut,&UI_MAIN_scanQueueIn,UI_MAIN_searchString,UI_MAIN_searchId,UI_MAIN_searchFlags);
    UI_MAIN_cpuNormal();
    vTaskDelete(NULL);
}

void UI_MAIN_scanSortTaskOneBook(){
    UI_MAIN_cpuFull();
    memset(&UI_MAIN_sortedFileIDs[0],0,sizeof(UI_MAIN_sortedFileIDs));
    FF_getList(&UI_MAIN_folderPath[0],&UI_MAIN_amountOfFiles,&UI_MAIN_sortedFileIDs[0],1,&UI_MAIN_scanQueueOut,&UI_MAIN_scanQueueIn,UI_MAIN_searchString,UI_MAIN_searchId,UI_MAIN_searchFlags);
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
#define UI_MAIN_RUN_SM_WAKEUP_TIMER_SETUP 12
#define UI_MAIN_RUN_SM_BOOK_SCAN 15 
#define UI_MAIN_RUN_SM_PAUSED 20
#define UI_MAIN_RUN_SM_SLEEP_TIMER_SETUP 22
#define UI_MAIN_RUN_SM_PLAY_INIT 24
#define UI_MAIN_RUN_SM_PLAYING 25

#define UI_MAIN_DEFAULT_SLEEP_TIME_S 300
SAVES_saveState_t UI_MAIN_saveState;
SAVES_settings_t UI_MAIN_settings;
char UI_MAIN_pcWriteBuffer[2048];

/**
  * @brief the main player state machine that only exits if the player should shut down
  *
  * @param startUpFlags flags that explain the type of power up (e.g. watchdog or manual)
  * 
  */
 void UI_MAIN_run(uint8_t startUpFlags){
    uint8_t mainSM=0;
    uint8_t pauseMode=0;
    uint16_t selectedFolder=0;
    uint16_t selectedFile=1;
    uint16_t oldSelectedFile=1;

    uint16_t currentPlayMinute=0;
    uint8_t currentPlaySecond=0;
    uint16_t allPlayMinute=0;
    uint8_t allPlaySecond=0;
    uint8_t percent=0;
    uint64_t currentPlayPosition=0;
    uint64_t currentPlaySize=0;
    uint64_t currentPlayOffset=0;
    uint64_t currentPlayBlockSize=0;
    uint64_t currentPlayBitrate=0;
    int32_t currentPlayChannels=0;
    uint64_t lastSdPlayMessage=0;
    uint64_t lastVolumeChangedTime=esp_timer_get_time()-5*1000000;
    uint64_t lastMinuteChangedTime=esp_timer_get_time()-5*1000000;
    uint64_t sleepTimeOffTime=0;//the timestamp when the currently setup sleeptime is reached
    uint32_t sleepTimeSetupS=UI_MAIN_DEFAULT_SLEEP_TIME_S;//the setup sleeptime
    uint64_t wakeupTimeSetupS=3600;
    uint16_t newPlayMinute=0;
    uint64_t newPlayPosition=0;
    uint8_t saveFlag=0;
    uint64_t lastAutoSave=0;
    UI_MAIN_keyMessages_t keyMessage;
    QueueHandle_t playerQueue=NULL;
    int32_t scanMessage=0;
    UI_MAIN_offTimestamp=esp_timer_get_time();

    int32_t searchId=-1;
    uint8_t searchFlags=0;
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
                        SCREENS_switchingOff(SD_CARD_getSize(),SAVES_getUsedSpaceLevel());
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
                        UI_MAIN_fwUpgradeRun();
                        mainSM=UI_MAIN_RUN_SM_FOLDER_SCAN;
                        UI_MAIN_searchFlags=NULL;
                        UI_MAIN_searchId=NULL;
                        UI_MAIN_searchString=NULL;
                        if(SAVES_getUsedSpaceLevel()>=75){
                            SAVES_cleanOldBookmarks();
                        }
                        if(SAVES_loadSettings(&UI_MAIN_settings)==0){
                            UI_MAIN_volume=UI_MAIN_settings.volume;
                            if(UI_MAIN_settings.sleepTimeSetupS<60){
                                UI_MAIN_settings.sleepTimeSetupS=UI_MAIN_DEFAULT_SLEEP_TIME_S;
                            }
                            sleepTimeSetupS=UI_MAIN_settings.sleepTimeSetupS;
                            if(UI_MAIN_settings.lastFolderName[0]!=0){
                                UI_MAIN_searchString=&UI_MAIN_settings.lastFolderName[0];
                                UI_MAIN_searchId=&searchId;
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
                    if(FF_getNameByID(SD_CARD_MOUNT_POINT,UI_MAIN_sortedBookIDs[selectedFolder-1],&UI_MAIN_selectedFolderName[0],0)!=0){
                        mainSM=UI_MAIN_RUN_SM_INIT;
                    }
                    SCREENS_folderSelect(selectedFolder,UI_MAIN_amountOfBooks,&UI_MAIN_selectedFolderName[0],UI_MAIN_getWakeupTimer());
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(50)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(selectedFolder>1){
                                selectedFolder--;
                            }else{
                                selectedFolder=UI_MAIN_amountOfBooks;
                            }
                            UI_ELEMENTS_textScrollyReset(2,0,12);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(selectedFolder<UI_MAIN_amountOfBooks){
                                selectedFolder++;
                            }else{
                                selectedFolder=1;
                            }
                            UI_ELEMENTS_textScrollyReset(2,0,12);
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_folderPath[0]=0;
                            strcpy(&UI_MAIN_folderPath[0],SD_CARD_MOUNT_POINT"/");
                            strcat(&UI_MAIN_folderPath[0],&UI_MAIN_selectedFolderName[0]);

                            searchFlags=0;
                            UI_MAIN_searchFlags=&searchFlags;
                            if(SAVES_existsBookmark(&UI_MAIN_selectedFolderName[0],&UI_MAIN_saveState)==0){
                                UI_MAIN_searchString=&UI_MAIN_saveState.fileName[0];
                                UI_MAIN_searchId=&searchId;
                            }

                            xTaskCreate(UI_MAIN_scanSortTaskOneBook, "UI_MAIN_scanSortTaskOneBook", 1024 * 5, NULL,  uxTaskPriorityGet(NULL), NULL);
                            mainSM=UI_MAIN_RUN_SM_BOOK_SCAN;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            mainSM=UI_MAIN_RUN_SM_WAKEUP_TIMER_SETUP;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){//TODO: go to a settings screen/setup, switch off for now
                            SCREENS_switchingOff(SD_CARD_getSize(),SAVES_getUsedSpaceLevel());
                            if(UI_MAIN_isImagePersisted()==0){
                                UI_MAIN_persistImage();
                            }
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            return; // returning to main means deep sleep
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_WAKEUP_TIMER_SETUP:
                    SCREENS_wakeupTimer(wakeupTimeSetupS);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if((now-lastMinuteChangedTime)>50000){
                                if(wakeupTimeSetupS>60){
                                    wakeupTimeSetupS-=60;
                                }else{
                                    wakeupTimeSetupS=60;
                                }
                            }else{
                                if(wakeupTimeSetupS>60*5){
                                    wakeupTimeSetupS-=60*5;
                                }else{
                                    wakeupTimeSetupS=60;
                                }
                            }
                            if(wakeupTimeSetupS<60) wakeupTimeSetupS=60;
                            lastMinuteChangedTime=now;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if((now-lastMinuteChangedTime)>50000){
                                wakeupTimeSetupS+=60;
                            }else{
                                wakeupTimeSetupS+=60*5;
                            }
                            if(wakeupTimeSetupS>3600*24) wakeupTimeSetupS=3600*24;
                            lastMinuteChangedTime=now;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            UI_MAIN_setWakeupTimer(wakeupTimeSetupS);
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            UI_MAIN_setWakeupTimer(0);
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            UI_MAIN_setWakeupTimer(0);
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        }
                    }
                    break;                    
            case UI_MAIN_RUN_SM_BOOK_SCAN:
                    if(xQueueReceive(UI_MAIN_scanQueueOut,&scanMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming scan messages
                        if(scanMessage==-1){//finished ok
                            if(UI_MAIN_amountOfFiles==0){//no files found, go back to folder selection
                                mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
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
                                    currentPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,currentPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                                }
                                UI_MAIN_screenOnIfNeeded();
                                UI_MAIN_searchString=NULL;
                                UI_MAIN_searchId=NULL;
                                searchId=-1;
                            }
                        }else if(scanMessage==-4){//scan canceled
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                        }else if(scanMessage>=0){
                            UI_MAIN_offTimestamp=esp_timer_get_time();
                            SCREENS_scanOneBook(scanMessage);
                        }else{//some kind of error
                            mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
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
                    if(pauseMode==0){//with chapter selection
                        FORMAT_HELPER_getPlayTimeByExtension(&currentPlaySecond,&currentPlayMinute,&percent,&UI_MAIN_selectedFileName[0],currentPlayPosition,currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,&allPlaySecond,&allPlayMinute,currentPlayChannels);
                        SCREENS_pause0(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],currentPlayMinute,currentPlaySecond,percent,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime));
                    }else if(pauseMode==1){//with time selection
                        uint64_t dummy;
                        newPlayPosition=FORMAT_HELPER_getFilePosByPlayTimeAndExtension(currentPlaySecond,newPlayMinute,&UI_MAIN_selectedFileName[0],currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,currentPlayChannels);
                        FORMAT_HELPER_getPlayTimeByExtension((uint8_t*)&dummy,(uint16_t*)&dummy,&percent,&UI_MAIN_selectedFileName[0],newPlayPosition,currentPlaySize,currentPlayOffset,currentPlayBlockSize,currentPlayBitrate,&allPlaySecond,&allPlayMinute,currentPlayChannels);
                        SCREENS_pause1(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],newPlayMinute,currentPlaySecond,percent,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime));
                    }
                    if((searchFlags&FF_AUTOSTART_FLAG)||(startUpFlags&UI_MAIN_STARTUPFLAG_RTC)){
                        searchFlags&=~FF_AUTOSTART_FLAG;//autostart only once for an enterd folder
                        startUpFlags&=~UI_MAIN_STARTUPFLAG_RTC;
                        mainSM=UI_MAIN_RUN_SM_PLAY_INIT;
                    }
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
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
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                                mainSM=UI_MAIN_RUN_SM_SLEEP_TIMER_SETUP;
                            }
                        }else if(pauseMode==1){//with time selection
                            if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                                currentPlaySecond=0;
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
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                                mainSM=UI_MAIN_RUN_SM_SLEEP_TIMER_SETUP;
                            }
                        }
                    }
                    break;
            case UI_MAIN_RUN_SM_SLEEP_TIMER_SETUP:
                    SCREENS_sleepTimer(sleepTimeSetupS);
                    while(xQueueReceive(UI_MAIN_keyQueue,&keyMessage,pdMS_TO_TICKS(10)) == pdPASS ){//wait for incoming key messages
                        if(keyMessage.keyEvent==UI_MAIN_KEY_MINUS){
                            if(sleepTimeSetupS>60){
                                sleepTimeSetupS-=60;
                            }else{
                                sleepTimeSetupS=60;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                            if(sleepTimeSetupS<99*60){
                                sleepTimeSetupS+=60;
                            }else{
                                sleepTimeSetupS=99*60;
                            }
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                            sleepTimeOffTime=esp_timer_get_time()+(sleepTimeSetupS*1000000);
                            mainSM=UI_MAIN_RUN_SM_PAUSED;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                            sleepTimeOffTime=0;
                            mainSM=UI_MAIN_RUN_SM_PAUSED;
                        }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                            sleepTimeOffTime=0;
                            mainSM=UI_MAIN_RUN_SM_PAUSED;
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
                        ESP_LOGI(UI_MAIN_LOG_TAG,"Trying to play %s at position %llu",UI_MAIN_sdPlayMsgSend.msgData,UI_MAIN_sdPlayMsgSend.filePos);
                        SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                        lastAutoSave=esp_timer_get_time();
                        lastSdPlayMessage=esp_timer_get_time();
                        allPlayMinute=0;
                        allPlaySecond=0;
                        mainSM=UI_MAIN_RUN_SM_PLAYING;
                    }else{//something went wrong
                        mainSM=UI_MAIN_RUN_SM_FOLDER_SELECTION;
                    }
                    pauseMode=0;
                    saveFlag=0;
                    xQueueAddToSet(UI_MAIN_keyQueue,UI_MAIN_keysAndPlay);
                    xQueueAddToSet(playerQueue,UI_MAIN_keysAndPlay);
                    break;
            case UI_MAIN_RUN_SM_PLAYING:
                    if(!UI_ELEMENTS_isDisplayOff()){
                        if(((now-lastVolumeChangedTime)/1000)<1000){
                            SCREENS_volumeChange(UI_MAIN_volume);
                        }else{
                            SCREENS_play(selectedFile,UI_MAIN_amountOfFiles,&UI_MAIN_selectedFolderName[0],currentPlayMinute,currentPlaySecond,percent,searchFlags,allPlayMinute,allPlaySecond,BATTERY_getCurrentVoltageStable(),UI_MAIN_timeDiffNowS(sleepTimeOffTime));
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
                                        if(searchFlags&FF_REPEAT_FLAG){
                                            mainSM=UI_MAIN_RUN_SM_PLAY_INIT;//go on if we should repeat
                                        }else{
                                            mainSM=UI_MAIN_RUN_SM_PAUSED;
                                        }
                                    }
                                    oldSelectedFile=selectedFile;
                                }else{//user pressed pause
                                    mainSM=UI_MAIN_RUN_SM_PAUSED;
                                    saveFlag=1;
                                }
                            }else if(UI_MAIN_sdPlayMsgRecv.msgType==SD_PLAY_MSG_TYPE_STOPPED_ERROR){
                                ESP_LOGI(UI_MAIN_LOG_TAG,"ERROR");
                                mainSM=UI_MAIN_RUN_SM_PAUSED;
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
                                if((now-lastVolumeChangedTime)>50000){
                                    if(UI_MAIN_volume>=0) UI_MAIN_volume-=5;
                                }else{
                                    if(UI_MAIN_volume>=0) UI_MAIN_volume-=100;
                                }
                                if(UI_MAIN_volume<0) UI_MAIN_volume=0;
                                if(UI_MAIN_volume>10000) UI_MAIN_volume=10000;
                                SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume);
                                lastVolumeChangedTime=now;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_PLUS){
                                if((now-lastVolumeChangedTime)>50000){
                                    if(UI_MAIN_volume<=10000) UI_MAIN_volume+=5;
                                }else{
                                    if(UI_MAIN_volume<=10000) UI_MAIN_volume+=100;
                                }
                                if(UI_MAIN_volume<0) UI_MAIN_volume=0;
                                if(UI_MAIN_volume>10000) UI_MAIN_volume=10000;
                                SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume);
                                lastVolumeChangedTime=now;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_SHORT_CLICK){
                                UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                pauseMode=1;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_LONG_CLICK){
                                UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                pauseMode=0;
                            }else if(keyMessage.keyEvent==UI_MAIN_KEY_DOUBLE_CLICK){
                                UI_MAIN_sdPlayMsgSend.msgType=SD_PLAY_MSG_TYPE_STOP_PLAY;
                                SD_PLAY_sendMessage(&UI_MAIN_sdPlayMsgSend,100);
                                skipped=1;
                            }
                        }

                    }while(rx!=NULL);
                    if((UI_MAIN_timeDiffNowS(sleepTimeOffTime)>0)&&(UI_MAIN_timeDiffNowS(sleepTimeOffTime)<=10)){//volume fade out if sleep timer nears end
                        SD_PLAY_volumeFilterSetVolume(UI_MAIN_volume-(UI_MAIN_volume/UI_MAIN_timeDiffNowS(sleepTimeOffTime)));
                    }
                    if((now-lastSdPlayMessage)/1000000>2){//2s no message from sd play
                        ESP_LOGE(UI_MAIN_LOG_TAG,"PLAYTIMEOUT");
                        mainSM=UI_MAIN_RUN_SM_PAUSED;
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
    if(dir){
        while ((currentEntry = readdir(dir)) != NULL) {
            uint16_t l=strlen(currentEntry->d_name);
            if((currentEntry->d_type!=DT_DIR)&&(l==15)){//only files are interesting fw file naming format: 01.02.03_ota.fw
                if(strncasecmp(&currentEntry->d_name[l-7],"_ota.fw",7)==0){
                    char majorStr[3];strncpy(&majorStr[0],&currentEntry->d_name[0],2);majorStr[2]=0;
                    char minorStr[3];strncpy(&minorStr[0],&currentEntry->d_name[3],2);minorStr[2]=0;
                    char patchStr[3];strncpy(&patchStr[0],&currentEntry->d_name[6],2);patchStr[2]=0;
                    uint8_t tmp=0;
                    if(UI_MAIN_hexStr2Byte(&majorStr[0],&tmp)) *major=tmp;
                    if(UI_MAIN_hexStr2Byte(&minorStr[0],&tmp)) *minor=tmp;
                    if(UI_MAIN_hexStr2Byte(&patchStr[0],&tmp)) *patch=tmp;
                    if (!((*major==FW_MAJOR)&&(*minor==FW_MINOR)&&(*patch==FW_PATCH))){//ignore currently installed FW
                        strcpy(fwFileName,&currentEntry->d_name[0]);
                        ESP_LOGI(UI_MAIN_LOG_TAG, "New firmware detected: v%i.%i.%i  file: %s",*major,*minor,*patch,fwFileName);
                        return;
                    }
                }
            }
        }
    }
    fwFileName[0]=0;
    ESP_LOGI(UI_MAIN_LOG_TAG, "No new firmware detected.");
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
        imageFlags|=8;
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
        ESP_LOGE(UI_MAIN_LOG_TAG,"Resetting config.");
        SAVES_spiffsFormat();
    }
    SAVES_init();
}

#endif
