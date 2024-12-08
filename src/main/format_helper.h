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
#ifndef FORMAT_HELPER_H
#define FORMAT_HELPER_H
/*
    This component helps in analyzing audio file formats to determine the
    important setup values to configure asp-adf pipeline correctly from the start
    and to determine file sizes and playpositions based on file position
*/
#include <stdint.h>

#define FORMAT_HELPER_FILE_TYPE_UNKNOWN 0
#define FORMAT_HELPER_FILE_TYPE_AMR 1
#define FORMAT_HELPER_FILE_TYPE_MP3 2
#define FORMAT_HELPER_FILE_TYPE_OGG 3
#define FORMAT_HELPER_FILE_TYPE_M4A 4


uint64_t FORMAT_HELPER_getFilePosByPlayTimeAndExtension(uint8_t currentPlaySecond,uint16_t currentPlayMinute,char* fileName,uint64_t currentPlaySize,uint64_t currentPlayOffset,uint64_t currentPlayBlockSize,uint64_t currentPlayBitrate,int32_t channels);
void FORMAT_HELPER_getPlayTimeByExtension(uint8_t* currentPlaySecond,uint16_t* currentPlayMinute,uint8_t* percent, char* fileName,uint64_t currentPlayPosition,uint64_t currentPlaySize,uint64_t currentPlayOffset,uint64_t currentPlayBlockSize,uint64_t currentPlayBitrate,uint8_t* allPlaySecond,uint16_t* allPlayMinute,int32_t channels);
uint8_t FORMAT_HELPER_getFileType(char* fileName);


uint8_t FORMAT_HELPER_getAMRFormatInformation(FILE* file,int32_t* sampleRate, int32_t* bits, int32_t* channels,uint64_t* offset,uint64_t* bitrate,uint64_t* blockSize,uint64_t fileSize,bool* upMix);
uint8_t FORMAT_HELPER_getMP3FormatInformation(FILE* file,int32_t* sampleRate, int32_t* bits, int32_t* channels,uint64_t* offset,uint64_t* bitrate,uint64_t* blockSize,uint64_t fileSize);
uint8_t FORMAT_HELPER_getOGGFormatInformation(FILE* file,int32_t* sampleRate, int32_t* bits, int32_t* channels,uint64_t* offset,uint64_t* bitrate,uint64_t* blockSize,uint64_t fileSize);
uint8_t FORMAT_HELPER_getM4AFormatInformation(FILE* file,int32_t* sampleRate, int32_t* bits, int32_t* channels,uint64_t* offset,uint64_t* bitrate,uint64_t* blockSize,uint64_t fileSize);



#endif
