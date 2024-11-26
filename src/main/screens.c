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
#include "ui_main.h"
#include "ui_elements.h"
#include "ff_handling.h"

/**
  * @brief shows a symbol to represent that no sd card was detected
  *
  */
 void SCREENS_noSdCard(){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(3);
    UI_ELEMENTS_update();
}

/**
  * @brief shows a symbol to represent that no audio book folder were found on the sd card
  *
  */
void SCREENS_noFolders(){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(5);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the audio book folder select screen and the currently selected folder
  *
  * @param selectedFolder the number of the currently selected audio book folder
  * @param amountOfFolder the amount of audio book folders
  * @param folderName pointer to the name of the selected audio book folder
  * @param secondsTime the wakeup time in seconds, needs to be >0 to be displayed
  * 
  */
void SCREENS_folderSelect(uint16_t selectedFolder,uint16_t amountOfFolders,char* folderName,uint64_t secondsTime){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(6);
    UI_ELEMENTS_numberSelect(3,2,selectedFolder,amountOfFolders,1);
    UI_ELEMENTS_textScrolly(2,0,12,folderName);
    if(secondsTime>0) UI_ELEMENTS_timeSelect(6,3,secondsTime,0);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the currently scanning all audio book folders screen
  *
  * @param deciPercent a value from 0...1000 as deci percent of the scan process
  * 
  */
void SCREENS_scanAllBooks(int32_t deciPercent){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(4);
    UI_ELEMENTS_progressBar(deciPercent/10);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the currently scanning a single audio book folder content screen
  *
  * @param deciPercent a value from 0...1000 as deci percent of the scan process
  * 
  */
void SCREENS_scanOneBook(int32_t deciPercent){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(7);
    UI_ELEMENTS_progressBar(deciPercent/10);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the paused screen if the player is paused/stopped and active chapter selection
  *
  * @param selectedFile the currently selected track/chapter/file number
  * @param amountOfFiles amount of all tracks/chapters/files
  * @param folderName pointer to the folderName of the currently active audio book
  * @param currentPlayMinute the current listening position minute
  * @param currentPlaySecond the current listening position second
  * @param percent percentage of listening position
  * @param batt current battery level in mV
  * @param sleepTimeSecondsLeft if >0 the seconds left of the sleep timer is displayed
  * 
  */
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

/**
  * @brief shows the paused screen if the player is paused/stopped and active time selection
  *
  * @param selectedFile the currently selected track/chapter/file number
  * @param amountOfFiles amount of all tracks/chapters/files
  * @param folderName pointer to the folderName of the currently active audio book
  * @param currentPlayMinute the current listening position minute
  * @param currentPlaySecond the current listening position second
  * @param percent percentage of listening position
  * @param batt current battery level in mV
  * @param sleepTimeSecondsLeft if >0 the seconds left of the sleep timer is displayed
  * 
  */
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

/**
  * @brief shows low battery symbol shortly before switching off
  *
  * @param batt current battery level in mV
  * 
  */
void SCREENS_lowBattery(uint32_t batt){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(12);
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the volume setup screen
  *
  * @param mode mode to display 0=volume, 1=play speed, 2=equalizer, 3=repeat settings
  * @param volume current volume level 0...10000
  * @param playSpeed current play speed setting 50...300 meaning x0.5...x3.0
  * @param equalizer a number representing the current equalizer preset 0=neutral equalizer
  * @param repeatMode currently selected repeatMode
  * 
  */
void SCREENS_playOverlay(uint8_t mode, int64_t volume,uint16_t playSpeed,uint8_t equalizer,uint8_t repeatMode,uint32_t sleepTimeSetupS,uint32_t sleepTimeOffTime){
  UI_ELEMENTS_cls();
  switch(mode){
    case 0: //volume
          UI_ELEMENTS_volume(volume);
          break;
    case 1: //sleep timer
          if(sleepTimeOffTime==0){//sleep timer is currently not running
            SCREENS_sleepTimer(sleepTimeOffTime);
          }else{
            SCREENS_sleepTimer(sleepTimeSetupS);
          }
          break;
    case 2: //play speed
          UI_ELEMENTS_playSpeed(playSpeed);
          break;
    case 3: //equalizer
          UI_ELEMENTS_equalizer(equalizer);
          break;
    case 4: //repeat mode
          UI_ELEMENTS_repeatMode(repeatMode);
          break;
  }
  UI_ELEMENTS_update();
}

/**
  * @brief shows the play screen during playing a track
  *
  * @param selectedFile the currently selected track/chapter/file number
  * @param amountOfFiles amount of all tracks/chapters/files
  * @param folderName pointer to the folderName of the currently active audio book
  * @param currentPlayMinute the current listening postion minute
  * @param currentPlaySecond the current listening postion second
  * @param percent percentage of listening postion
  * @param repeatMode special flags for the current audio book
  * @param allPlayMinute minutes length of the current track
  * @param allPlaySecond seconds length of the current track
  * @param batt current battery level in mV
  * @param sleepTimeSecondsLeft if >0 the seconds left of the sleep timer is displayed
  * @param playOverlaySecondsLeft if >0 the seconds left of the sub menu state within the play overlay screen
  * 
  */
void SCREENS_play(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint8_t repeatMode,uint16_t allPlayMinute,uint8_t allPlaySecond,uint32_t batt,uint32_t sleepTimeSecondsLeft,uint32_t playOverlaySecondsLeft){
    uint64_t now=esp_timer_get_time();
    char b[3]={0};
    UI_ELEMENTS_cls();
    UI_ELEMENTS_progressBar(percent);
    if((allPlayMinute+allPlaySecond>0)&&(((now/1000)%8000)<2000)){
        UI_ELEMENTS_timePlayed((allPlayMinute*60+allPlaySecond)-(currentPlayMinute*60+currentPlaySecond),2);
    }else{
        UI_ELEMENTS_timePlayed(currentPlayMinute*60+currentPlaySecond,0);
    }
    UI_ELEMENTS_textScrolly(0,0,14,&folderName[0]);
    UI_ELEMENTS_numberSelect(0,1,selectedFile,amountOfFiles,0);
    if(repeatMode&UI_MAIN_REPEAT_FOLDER){
        UI_ELEMENTS_mainSymbol(13);
    }else{
        UI_ELEMENTS_mainSymbol(2);
    }
    playOverlaySecondsLeft/=1000;
    if((playOverlaySecondsLeft)>0){
      b[0]=10-(playOverlaySecondsLeft%10)+48;
      UI_ELEMENTS_textScrolly(8,3,1,&b[0]);
    }
    UI_ELEMENTS_batteryIndicator(batt);
    UI_ELEMENTS_sleepTimeLeft(sleepTimeSecondsLeft);
    UI_ELEMENTS_update();
}

/**
  * @brief shows a symbol to represent that the player is currently shutting down
  * @param sdSizeMB raw size of the SD card in MB
  * @param SPIFFSUsagePercent SPIFSS fill level in % 0..100
  */
void SCREENS_switchingOff(uint64_t sdSizeMB,uint8_t SPIFFSUsagePercent,int32_t numberOfBookmarks){
    char s[20];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(8);
    sprintf(&s[0],"%7lluMB",sdSizeMB);
    UI_ELEMENTS_textScrolly(3,2,12,&s[0]);
    sprintf(&s[0],"%6li/%d%%",numberOfBookmarks,SPIFFSUsagePercent);
    UI_ELEMENTS_textScrolly(2,3,16,&s[0]);
    UI_ELEMENTS_update();
}

/**
  * @brief shows the inital fw upgrade notification screen
  *
  * @param major new major version
  * @param minor new minor version
  * @param patch new patch version
  * @param timeout seconds left until an upgrade can be started
  * 
  */
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

/**
  * @brief shows the symbol during system start
  *
  * @param imageFlags specific firmware flags (e.g. if persisted)
  * 
  */
void SCREENS_startUp(uint8_t imageFlags){
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(9);
    //sprintf(&b[0],"TINAMP");UI_ELEMENTS_textScrolly(4,0,6,&b[0]);
    if(imageFlags&8){
        sprintf(&b[0],"ok");
        UI_ELEMENTS_textScrolly(7,2,2,&b[0]);
    }else{
        sprintf(&b[0],"TEST");
        UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
    }
    UI_ELEMENTS_update();
}

/**
  * @brief shows the screen during a running fw upgrade
  *
  * @param percent percent of finished fw upgrade 0...100
  * 
  */
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

/**
  * @brief shows the setup screen for setting the sleep timer
  *
  * @param secondsLeft currently selected sleep time in s
  * 
  */
void SCREENS_sleepTimer(uint32_t secondsLeft){
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(8);
    if(secondsLeft==0){
        sprintf(&b[0],"-");
        UI_ELEMENTS_textScrolly(7,2,1,&b[0]);
    }else{
      sprintf(&b[0],"m");
      UI_ELEMENTS_numberSelect(0,2,secondsLeft/60,480,1);
      UI_ELEMENTS_textScrolly(10,2,1,&b[0]);
    }
    UI_ELEMENTS_update();
}

/**
  * @brief shows the setup screen for setting the wake up timer
  *
  * @param secondsLeft currently selected wake up time in s
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_wakeupTimer(uint64_t secondsTime,uint8_t blinkMode){
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(16);
    if(blinkMode==0){
      UI_ELEMENTS_timeSelect(6,3,secondsTime,0);
    }else{
      UI_ELEMENTS_timeSelect(6,3,secondsTime,1);
      UI_ELEMENTS_update();
    }
}

/**
  * @brief shows the setup screen for setting the screen rotation
  *
  * @param rotation 0=normal, 1=rotated
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_screenSetup(uint8_t rotation,uint8_t blinkMode){
    uint64_t now=esp_timer_get_time();
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(17);
    if(rotation){
      sprintf(&b[0],"180");
    }else{
      sprintf(&b[0]," 0 ");
    }
    if(blinkMode==0){
        UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
    }else{
      if(((now/1000)%1000)<100){
          sprintf(&b[0],"   ");
      }
      UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
      UI_ELEMENTS_update();
    }
}

/**
  * @brief shows the setup screen for setting the screen brightness
  *
  * @param brightness 0=off/minimal, 255=maximum
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_screenBrightnessSetup(uint8_t brightness,uint8_t blinkMode){
    uint64_t now=esp_timer_get_time();
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(21);
    sprintf(&b[0],"%3i",brightness);
    if(blinkMode==0){
        UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
    }else{
      if(((now/1000)%1000)<100){
          sprintf(&b[0],"   ");
      }
      UI_ELEMENTS_textScrolly(6,2,4,&b[0]);
      UI_ELEMENTS_update();
    }
}

/**
  * @brief shows the setup screen for setting the rotation direction of the
  *        rotary encoder
  *
  * @param dir 0=normal/factory default, 1=reversed direction
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_rotDirSetup(uint8_t dir,uint8_t blinkMode){
    uint64_t now=esp_timer_get_time();
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(18);
    if(dir){
      sprintf(&b[0],"+ <--");
    }else{
      sprintf(&b[0],"--> +");
    }
    if(blinkMode==0){
        UI_ELEMENTS_textScrolly(8,2,5,&b[0]);
    }else{
      if(((now/1000)%1000)<100){
          sprintf(&b[0],"     ");
      }
      UI_ELEMENTS_textScrolly(8,2,5,&b[0]);
      UI_ELEMENTS_update();
    }
}

/**
  * @brief shows the setup screen for setting the rotation speed of the
  *        rotary encoder
  *
  * @param speed 0=1x, 1=0.5x
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_rotSpeedSetup(uint8_t speed,uint8_t blinkMode){
    uint64_t now=esp_timer_get_time();
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(19);
    if(speed){
      sprintf(&b[0],"x0.5");
    }else{
      sprintf(&b[0],"  x1");
    }
    if(blinkMode==0){
      UI_ELEMENTS_textScrolly(8,2,5,&b[0]);
    }else{
      if(((now/1000)%1000)<100){
          sprintf(&b[0],"     ");
      }
      UI_ELEMENTS_textScrolly(8,2,5,&b[0]);
      UI_ELEMENTS_update();
    }
}

/**
  * @brief shows the setup screen for deleting the bookmarks
  *
  * @param numberOfFiles the number of bookmarks that are stored
  * @param blinkMode 1=numbers blink, 0=no blinking
  * 
  */
void SCREENS_bookmarkDeletionSetup(int32_t numberOfFiles,uint8_t blinkMode){
    uint64_t now=esp_timer_get_time();
    char b[10];
    UI_ELEMENTS_cls();
    UI_ELEMENTS_mainSymbol(20);
    sprintf(&b[0],"%6li",numberOfFiles);
    if(blinkMode==0){
      UI_ELEMENTS_textScrolly(0,2,6,&b[0]);
    }else{
      UI_ELEMENTS_textScrolly(0,2,6,&b[0]);
      if(((now/1000)%1000)<100){
          sprintf(&b[0],"   ");
          UI_ELEMENTS_textScrolly(7,3,3,&b[0]);
      }
      UI_ELEMENTS_update();
    }
}

#endif
