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
#ifndef SAVES_H
#define SAVES_H
/*
    This component handles the storing and restoring of bookmarks
    and the settings if the player itself e.g. volume
    It also takes care of the SPIFFS handling
*/
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "ff_handling.h"

typedef struct{
    uint8_t version;
    uint16_t playMinute;
    uint8_t playSecond;
    uint64_t playPosition;
    uint64_t playSize;
    uint64_t playOffset;
    uint64_t playBlockSize;
    uint64_t playBitrate;
    int32_t playChannels;
    char fileName[FF_FILE_PATH_MAX];
    char folderName[FF_FILE_PATH_MAX];
    uint16_t playSpeed;
    uint16_t equalizer;
    uint8_t repeatMode;
    uint8_t finished;
}SAVES_saveState_t;


typedef struct{
    uint8_t version;
    int64_t volume;
    char lastFolderName[FF_FILE_PATH_MAX];
    uint32_t sleepTimeSetupS;
    uint8_t screenRotation;
    uint8_t rotaryEncoderSpeed;
    uint8_t rotaryEncoderDirection;
    uint8_t brightness;
    uint8_t reducedMode;
}SAVES_settings_t;


void SAVES_spiffsFormat();
void SAVES_init();

void SAVES_deInit();
uint8_t SAVES_loadSettings(SAVES_settings_t *settings);
uint8_t SAVES_saveSettings(SAVES_settings_t *settings);
void SAVES_getBookmarkFileFromFolderName(char* folderName,char* saveFileName);
uint8_t SAVES_existsBookmark(char* folderName,SAVES_saveState_t* save);
uint8_t SAVES_saveBookmark(char* folderName,SAVES_saveState_t* save);
int32_t SAVES_cleanOldBookmarks(uint8_t flags);
int32_t SAVES_getUsedSpaceLevel();

#endif
