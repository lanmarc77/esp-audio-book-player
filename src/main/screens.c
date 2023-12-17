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
#ifndef SCREENS_C
#define SCREENS_C
#include "esp_timer.h"
#include "screens.h"
#include "ui_elements.h"
#include "ff_handling.h"

void SCREENS_noSdCard(){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(3);
    UI_ELEMENTS_update();
}

void SCREENS_noFolders(){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(5);
    UI_ELEMENTS_update();
}

void SCREENS_folderSelect(uint16_t selectedFolder,uint16_t amountOfFolders,char* folderName){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(6);
    UI_ELEMENTS_numberSelect(3,2,selectedFolder,amountOfFolders,1);
    UI_ELEMENTS_textScrolly(2,0,12,folderName);
    UI_ELEMENTS_update();
}

void SCREENS_scanAllBooks(int32_t deciPercent){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(4);
    UI_ELEMENTS_progressBar(deciPercent/10);
    UI_ELEMENTS_update();
}

void SCREENS_scanOneBook(int32_t deciPercent){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(7);
    UI_ELEMENTS_progressBar(deciPercent/10);
    UI_ELEMENTS_update();
}
void SCREENS_pause0(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint32_t batt,uint32_t sleepTimeSecondsLeft){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_progressBar(percent);
    UI_ELEMENTS_timePlayed(currentPlayMinute*60+currentPlaySecond,0);
    UI_ELEMENTS_textScrolly(0,0,14,&folderName[0]);
    UI_ELEMENTS_numberSelect(0,1,selectedFile,amountOfFiles,1);
    UI_ELEMENTS_mainSymbol(1);
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_sleepTimeLeft(sleepTimeSecondsLeft);
    UI_ELEMENTS_update();
}

void SCREENS_pause1(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint32_t batt,uint32_t sleepTimeSecondsLeft){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_progressBar(percent);
    UI_ELEMENTS_timePlayed(currentPlayMinute*60+currentPlaySecond,1);
    UI_ELEMENTS_textScrolly(0,0,14,&folderName[0]);
    UI_ELEMENTS_numberSelect(0,1,selectedFile,amountOfFiles,0);
    UI_ELEMENTS_mainSymbol(1);
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_sleepTimeLeft(sleepTimeSecondsLeft);
    UI_ELEMENTS_update();
}

void SCREENS_lowBattery(uint32_t batt){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(12);
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_update();
}

void SCREENS_volumeChange(int64_t volume){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_volume(volume);
    UI_ELEMENTS_update();
}

void SCREENS_play(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint8_t searchFlags,uint16_t allPlayMinute,uint8_t allPlaySecond,uint32_t batt,uint32_t sleepTimeSecondsLeft){
    uint64_t now=esp_timer_get_time();
    UI_ELEMENTS_cls();
    UI_ELEMENTS_progressBar(percent);
    if((allPlayMinute+allPlaySecond>0)&&(((now/1000)%8000)<2000)){
        UI_ELEMENTS_timePlayed((allPlayMinute*60+allPlaySecond)-(currentPlayMinute*60+currentPlaySecond),2);
    }else{
        UI_ELEMENTS_timePlayed(currentPlayMinute*60+currentPlaySecond,0);
    }
    UI_ELEMENTS_textScrolly(0,0,14,&folderName[0]);
    UI_ELEMENTS_numberSelect(0,1,selectedFile,amountOfFiles,0);
    if(searchFlags&FF_REPEAT_FLAG){
        UI_ELEMENTS_mainSymbol(13);
    }else{
        UI_ELEMENTS_mainSymbol(2);
    }
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_sleepTimeLeft(sleepTimeSecondsLeft);
    UI_ELEMENTS_update();
}

void SCREENS_switchingOff(){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(8);
    UI_ELEMENTS_update();
}

void SCREENS_fwUpgradeInit(uint8_t major,uint8_t minor,uint8_t patch,int8_t timeout){
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(14);
    sprintf(&b[0],"%02X.%02X.%02X",major,minor,patch);
    UI_ELEMENTS_textScrolly(7,3,8,&b[0]);
    if(timeout>=0){
        sprintf(&b[0],"%02i",timeout);
        UI_ELEMENTS_textScrolly(10,2,2,&b[0]);
    }
    UI_ELEMENTS_update();
}

void SCREENS_startUp(uint8_t imageFlags){
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(9);
    if(imageFlags&8){
        sprintf(&b[0],"ok");
        UI_ELEMENTS_textScrolly(7,2,2,&b[0]);
    }else{
        sprintf(&b[0],"TEST");
        UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
    }
    UI_ELEMENTS_update();
}

void SCREENS_fwUpgradeRunning(int8_t percent){
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(15);
    if(percent>=0){
        sprintf(&b[0],"%3i%%",percent);
        UI_ELEMENTS_textScrolly(6,1,4,&b[0]);
    }else if(percent==-1){//ok finished
    }else if(percent==-2){//error finished
        sprintf(&b[0],"XXXX");
        UI_ELEMENTS_textScrolly(6,1,4,&b[0]);
        UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
    }
    UI_ELEMENTS_update();
}

void SCREENS_sleepTimer(uint32_t secondsLeft){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(8);
    UI_ELEMENTS_numberSelect(0,2,secondsLeft/60,99,1);
    UI_ELEMENTS_update();
}
#endif
