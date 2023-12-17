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
#ifndef UI_MAIN_H
#define UI_MAIN_H
#include <stdint.h>

void UI_MAIN_init();

//the main UI player logic
void UI_MAIN_run();

void UI_MAIN_deInit();

uint8_t UI_MAIN_isImagePersisted();
void UI_MAIN_persistImage();
void UI_MAIN_fwUpgradeRun();

#endif
