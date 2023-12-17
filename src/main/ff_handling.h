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
#ifndef FF_HANDLING_H
#define FF_HANDLING_H
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define FF_MAX_SORT_ELEMENTS 1010
#define FF_FILE_PATH_MAX 256
#define FF_REPEAT_FLAG 1
#define FF_AUTOSTART_FLAG 2


//ffType 0=directory, 1=files
//flags: bit0=repeat.dat found in folder
uint8_t FF_getList(char* folderPath,uint16_t* amountOfEntries,uint16_t* sortedIdArray,uint8_t ffType,QueueHandle_t* outQueue, QueueHandle_t* inQueue, char* searchString,int32_t* searchId,uint8_t* flags);
//ffType 0=directory, 1=files
uint8_t FF_getNameByID(char* folderBasePath,uint16_t ID,char *resultName,uint8_t ffType);


#endif
