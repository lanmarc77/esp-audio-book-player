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
#ifndef SD_PLAY_H
#define SD_PLAY_H
/*
    This component handles the esp-adf pipelines and the whole playing process
    It uses queues for communication and spawns own tasks
*/
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

//all msgTypes
#define SD_PLAY_MSG_TYPE_START_PLAY 1
#define SD_PLAY_MSG_TYPE_STOP_PLAY 2
#define SD_PLAY_MSG_TYPE_STOPPED_NO_ERROR 3
#define SD_PLAY_MSG_TYPE_STOPPED_ERROR 4
#define SD_PLAY_MSG_TYPE_FILEPOS_STATE 5

//using a HW mute pin on PCM5102 or UDA1334a did not change the
//pop behavior when I²S is reconfigured, so no need to use at the moment
//#define SD_PLAY_HW_MUTE_PIN -1

typedef struct {
    uint8_t msgType;
    char* msgData;
    uint64_t filePos;
    uint64_t offset;
    uint64_t blockSize;
    uint32_t bitrate;
    uint64_t fileSize;
    int32_t channels;
} SD_PLAY_message_t;

void SD_PLAY_init();
uint8_t SD_PLAY_startService();
uint8_t SD_PLAY_sendMessage(SD_PLAY_message_t* msg, uint16_t waitTime);
uint8_t SD_PLAY_getMessage(SD_PLAY_message_t* msg, uint16_t waitTime);
void SD_PLAY_volumeFilterSetVolume(int64_t volume);
QueueHandle_t SD_PLAY_getMessageQueue();

#endif
