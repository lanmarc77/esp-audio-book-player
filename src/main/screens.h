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
#ifndef SCREENS_H
#define SCREENS_H
#include <stdint.h>

void SCREENS_noSdCard();
void SCREENS_noFolders();
void SCREENS_folderSelect(uint16_t selectedFolder,uint16_t amountOfFolders,char* folderName);
void SCREENS_scanAllBooks(int32_t deciPercent);
void SCREENS_scanOneBook(int32_t deciPercent);
void SCREENS_pause0(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint32_t batt,uint32_t sleepTimeSecondsLeft);
void SCREENS_pause1(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint32_t batt,uint32_t sleepTimeSecondsLeft);
void SCREENS_lowBattery(uint32_t batt);
void SCREENS_startUp(uint8_t imageFlags);
void SCREENS_volumeChange(int64_t volume);
void SCREENS_play(uint16_t selectedFile,uint16_t amountOfFiles,char* folderName,uint16_t currentPlayMinute,uint8_t currentPlaySecond,uint8_t percent,uint8_t searchFlags,uint16_t allPlayMinute,uint8_t allPlaySecond,uint32_t batt,uint32_t sleepTimeSecondsLeft);
void SCREENS_fwUpgradeInit(uint8_t major,uint8_t minor,uint8_t patch,int8_t timeout);
void SCREENS_fwUpgradeRunning(int8_t percent);
void SCREENS_switchingOff();
void SCREENS_sleepTimer(uint32_t secondsLeft);

#endif
