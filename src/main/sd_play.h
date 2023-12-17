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
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define SD_PLAY_MSG_TYPE_START_PLAY 1
#define SD_PLAY_MSG_TYPE_STOP_PLAY 2
#define SD_PLAY_MSG_TYPE_STOPPED_NO_ERROR 3
#define SD_PLAY_MSG_TYPE_STOPPED_ERROR 4
#define SD_PLAY_MSG_TYPE_FILEPOS_STATE 5

typedef struct {
    uint8_t msgType;
    char* msgData;
    //using offset, blockSize and filePos the current timestamp can be calculated as follows
    //for AMR: each block is exactly 20ms long as per format definition
    //         timestamp in s=(filePos-offset/blockSize)*0.02;

    //for MP3: timestamp in s = (((filePos-offset)*8)/bitrateInkBitPerS)/1000;
    //              blockSize = ( 1152 / 8 ) * bitrateInkBitPerS / SampleRateinKhz
    uint64_t filePos;
    uint64_t offset;
    uint64_t blockSize;
    uint32_t bitrate;
    uint64_t fileSize;
    int32_t channels;
} SD_PLAY_message_t;


//returns 0 on success
uint8_t SD_PLAY_startService();

//returns 0 on success
uint8_t SD_PLAY_sendMessage(SD_PLAY_message_t* msg, uint16_t waitTime);

//returns 0 on success, 1 for timeout
uint8_t SD_PLAY_getMessage(SD_PLAY_message_t* msg, uint16_t waitTime);

//volume 0 ... 10000
void SD_PLAY_volumeFilterSetVolume(int64_t volume);

QueueHandle_t SD_PLAY_getMessageQueue();

#endif
