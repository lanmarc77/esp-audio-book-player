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
#ifndef UI_ELEMENTS_H
#define UI_ELEMENTS_H
#include <stdint.h>
#include <stdbool.h>

#define FW_MAJOR 00
#define FW_MINOR 00
#define FW_PATCH 03

void UI_ELEMENTS_init();
void UI_ELEMENTS_textScrolly(uint8_t x,uint8_t y, uint8_t length, char *string);
void UI_ELEMENTS_textScrollyReset(uint8_t x,uint8_t y, uint8_t length);
void UI_ELEMENTS_numberSelect(uint8_t x,uint8_t y,uint16_t selectedNumber,uint16_t amountOfNumbers,uint8_t blinkMode);
void UI_ELEMENTS_mainSymbol(uint8_t symbol);
void UI_ELEMENTS_batteryIndicator(uint32_t value);
//displayMode: 0=normal,1=hours+minutes blink,2=playedS is displayed as negative number
void UI_ELEMENTS_timePlayed(int32_t playedS,uint8_t displayMode);
void UI_ELEMENTS_progressBar(uint8_t percent);
void UI_ELEMENTS_update();
void UI_ELEMENTS_displayOn();
void UI_ELEMENTS_displayOff();
bool UI_ELEMENTS_isDisplayOff();
void UI_ELEMENTS_cls();
void UI_ELEMENTS_volume(int64_t volume);
void UI_ELEMENTS_sleepTimeLeft(uint32_t secondsLeft);

#endif
