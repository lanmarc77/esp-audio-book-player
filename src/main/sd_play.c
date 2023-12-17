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
#ifndef SD_PLAY_C
#define SD_PLAY_C
#include <esp_log.h>
#include <string.h>
#include <sys/stat.h>
#include "soc/io_mux_reg.h"
#include "sd_play.h"
#include "format_helper.h"

#include "board.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "amr_decoder.h"
#include "aac_decoder.h"
#include "ogg_decoder.h"
#include "audio_mutex.h"
#include "audio_error.h"
#include "audio_thread.h"

#define SD_PLAY_LOG_TAG "SD_PLAY"
QueueHandle_t SD_PLAY_inQueue=NULL;
QueueHandle_t SD_PLAY_outQueue=NULL;


FILE* SD_PLAY_currentPlayfile=NULL;
uint64_t SD_PLAY_currentFileSize=0;
fpos_t SD_PLAY_currentFilePos=0;
fpos_t SD_PLAY_desiredFilePos=0;

audio_pipeline_handle_t SD_PLAY_pipeline=NULL;
audio_element_handle_t SD_PLAY_i2sStreamWriter=NULL, SD_PLAY_musicFormatDecoder=NULL,SD_PLAY_volumeFilter=NULL;
audio_event_iface_handle_t SD_PLAY_eventChannel=NULL;
//set true if the ESP32 codec could figure out bitrate, bits and samplerates, used to determine when it is ok to seek to also bad file postions
bool SD_PLAY_codecSyned=false;
//assures as long the current pipeline is not stopped that no other play can start
SemaphoreHandle_t SD_PLAY_semStopPlaying;


//internal states
//stopped=0
//starting (ignores any stop or start requests but measures if running is not happening)=1
//running=2
//stopping (ignores any stop or start requests but measures if stopping is not happening)->reset if timeout=3
uint8_t SD_PLAY_internalSM=0;

QueueHandle_t SD_PLAY_getMessageQueue(){
    return SD_PLAY_outQueue;
}

uint8_t SD_PLAY_sendMessageOut(SD_PLAY_message_t* msg, uint16_t waitTime){
    if(SD_PLAY_outQueue==NULL) return 1;
    if(xQueueSend(SD_PLAY_outQueue,msg,pdMS_TO_TICKS(waitTime))!=pdPASS){
        ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR COULD NOT INSERT IN outQUEUE %d",msg->msgType);
        return 2;
    }
    return 0;
}


#define SD_PLAY_VOLUMEFILTER_BUFFER_SIZE 128

esp_err_t SD_PLAY_volumeFilterDestroy(audio_element_handle_t self) {
    return ESP_OK; 
}

esp_err_t SD_PLAY_volumeFilterOpen(audio_element_handle_t self) {
    return ESP_OK;
}

esp_err_t SD_PLAY_volumeFilterClose(audio_element_handle_t self) {
  return ESP_OK; 
}

int64_t SD_PLAY_currentVolume = 0;

void SD_PLAY_volumeFilterSetVolume(int64_t volume) {
    if(volume>10000) volume=10000;
    if(volume<0) volume=0;
    volume=(volume*volume*volume*volume)/1000000000000;
    SD_PLAY_currentVolume = volume;
}

uint16_t SD_PLAY_waitJump=0;//if 0 no need to wait, if >1 wait a bit
bool SD_PLAY_stereoUpMix=false;
char SD_PLAY_stereoUpMixBuffer[SD_PLAY_VOLUMEFILTER_BUFFER_SIZE*2+10];
int SD_PLAY_volumeFilterProcess(audio_element_handle_t self, char *buffer, int size)
{
    audio_element_info_t SD_PLAY_volumeFilterInfo;
    audio_element_getinfo(self, &SD_PLAY_volumeFilterInfo);

    int64_t internalVolume=SD_PLAY_currentVolume;

    int size_read = audio_element_input(self, buffer, size);
    int result = size_read; // Could be negative error code
    //ESP_LOGI(SD_PLAY_LOG_TAG,"VOL: size %d",size);
    if(SD_PLAY_desiredFilePos!=0){
        internalVolume=0;
    }else{
        if(SD_PLAY_waitJump){
            if(SD_PLAY_waitJump<500){
                internalVolume=0;
                SD_PLAY_waitJump++;
            }
        }
    }
    if (size_read > 0) {
        if(SD_PLAY_volumeFilterInfo.bits==8){
            int8_t *samples = (int8_t *)buffer;
            int8_t *samplesS = (int8_t *)SD_PLAY_stereoUpMixBuffer;
            size_t count = size;
            for (size_t i = 0; i < count; i++) {
                samples[i] = (samples[i] * internalVolume)/10000;
                if(SD_PLAY_stereoUpMix){
                    samplesS[i*2] = samples[i];
                    samplesS[i*2+1] = samples[i];
                }
            }
        }else if(SD_PLAY_volumeFilterInfo.bits==16){
            int16_t *samples = (int16_t *) buffer;
            int16_t *samplesS = (int16_t *) SD_PLAY_stereoUpMixBuffer;
            size_t count = size / 2;
            for (size_t i = 0; i < count; i++) {
                samples[i] = (samples[i] * internalVolume)/10000;
                if(SD_PLAY_stereoUpMix){
                    samplesS[i*2] = samples[i];
                    samplesS[i*2+1] = samples[i];
                }
            }
        }else if(SD_PLAY_volumeFilterInfo.bits==32){
            int32_t *samples = (int32_t *) buffer;
            int32_t *samplesS = (int32_t *) SD_PLAY_stereoUpMixBuffer;
            size_t count = size / 4;
            for (size_t i = 0; i < count; i++) {
                samples[i] = (samples[i] * internalVolume)/10000;
                if(SD_PLAY_stereoUpMix){
                    samplesS[i*2] = samples[i];
                    samplesS[i*2+1] = samples[i];
                }
            }
        }
        int size_write=0;
        if(SD_PLAY_stereoUpMix){
            size_write = audio_element_output(self, SD_PLAY_stereoUpMixBuffer, size*2);
        }else{
            size_write = audio_element_output(self, buffer, size_read);
        }
        result = size_write;
    }
    return result;
}

uint64_t SD_PLAY_blockSize=0;
uint64_t SD_PLAY_offset=0;
uint32_t SD_PLAY_bitrate=0;
int32_t SD_PLAY_channels;

uint8_t SD_PLAY_m4aSM=0;
int SD_PLAY_m4aReadCb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    fpos_t fpos;
    if(!SD_PLAY_currentPlayfile) return AEL_IO_DONE;
    if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
        return AEL_IO_FAIL;
    }
    SD_PLAY_currentFilePos=fpos;
    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    int readBytes=0;
    //ESP_LOGI(SD_PLAY_LOG_TAG,"CB: request %i SM %i  byte_pos: %llu, sync:%d",len,SD_PLAY_m4aSM,info.byte_pos,SD_PLAY_codecSyned);
    switch(SD_PLAY_m4aSM){
        default:    if((SD_PLAY_desiredFilePos>0)&&(SD_PLAY_codecSyned)) {
                        fseek(SD_PLAY_currentPlayfile,SD_PLAY_desiredFilePos,SEEK_SET);
                        SD_PLAY_desiredFilePos=0;
                        SD_PLAY_waitJump=1;
                    }
                    readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                    break;
    }
    if(readBytes<=0){
        if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
            return AEL_IO_FAIL;
        }
        SD_PLAY_currentFilePos=fpos;
        fclose(SD_PLAY_currentPlayfile);
        SD_PLAY_currentPlayfile=NULL;
        SD_PLAY_m4aSM=0;
        return AEL_IO_DONE;
    }
    return readBytes;
}

uint8_t SD_PLAY_oggSM=0;
int SD_PLAY_oggReadCb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    fpos_t fpos;
    if(!SD_PLAY_currentPlayfile) return AEL_IO_DONE;
    if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
        return AEL_IO_FAIL;
    }
    SD_PLAY_currentFilePos=fpos;
    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    int readBytes=0;
    //ESP_LOGI(SD_PLAY_LOG_TAG,"CB: request %i SM %i  byte_pos: %llu",len,SD_PLAY_oggSM,info.byte_pos);
    switch(SD_PLAY_oggSM){
        default:    if((SD_PLAY_desiredFilePos>0)&&(SD_PLAY_codecSyned)) {
                        fseek(SD_PLAY_currentPlayfile,SD_PLAY_desiredFilePos,SEEK_SET);
                        SD_PLAY_desiredFilePos=0;
                        SD_PLAY_waitJump=1;
                    }
                    readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                    break;
    }
    if(readBytes<=0){
        if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
            return AEL_IO_FAIL;
        }
        SD_PLAY_currentFilePos=fpos;
        fclose(SD_PLAY_currentPlayfile);
        SD_PLAY_currentPlayfile=NULL;
        SD_PLAY_oggSM=0;
        return AEL_IO_DONE;
    }
    return readBytes;
}


uint8_t SD_PLAY_mp3SM=0;
int SD_PLAY_mp3ReadCb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    fpos_t fpos;
    if(!SD_PLAY_currentPlayfile) return AEL_IO_DONE;
    if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
        return AEL_IO_FAIL;
    }
    SD_PLAY_currentFilePos=fpos;
    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    int readBytes=0;
    //ESP_LOGI(SD_PLAY_LOG_TAG,"CB: request %i SM %i  byte_pos: %llu",len,SD_PLAY_mp3SM,info.byte_pos);
    switch(SD_PLAY_mp3SM){
        default:    if((SD_PLAY_desiredFilePos>0)&&(SD_PLAY_codecSyned)) {
                        fseek(SD_PLAY_currentPlayfile,SD_PLAY_desiredFilePos,SEEK_SET);
                        SD_PLAY_desiredFilePos=0;
                        SD_PLAY_waitJump=1;
                    }
                    readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                    break;
    }
    if(readBytes<=0){
        if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
            return AEL_IO_FAIL;
        }
        SD_PLAY_currentFilePos=fpos;
        fclose(SD_PLAY_currentPlayfile);
        SD_PLAY_currentPlayfile=NULL;
        SD_PLAY_mp3SM=0;
        return AEL_IO_DONE;
    }
    return readBytes;
}

uint8_t SD_PLAY_amrSM=0;

int SD_PLAY_amrReadCb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    fpos_t fpos;
    if(!SD_PLAY_currentPlayfile) return AEL_IO_DONE;
    if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
        return AEL_IO_FAIL;
    }
    SD_PLAY_currentFilePos=fpos;
    audio_element_info_t info = {0};
    audio_element_getinfo(el, &info);
    int readBytes=0;
    //ESP_LOGI(SD_PLAY_LOG_TAG,"CB: request %i SM %i",len,SD_PLAY_amrSM);
    switch(SD_PLAY_amrSM){
        case 0: SD_PLAY_offset=0;
                SD_PLAY_blockSize=0;
                if(len==6){
                    readBytes=fread(buf,1,6,SD_PLAY_currentPlayfile);
                    SD_PLAY_amrSM=1;
                }else{
                    readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                    SD_PLAY_amrSM=10;
                }
                break;
        case 1: if(len==3){//AMR-WB if this is entered
                    readBytes=fread(buf,1,3,SD_PLAY_currentPlayfile);
                    SD_PLAY_amrSM=2;
                    SD_PLAY_offset=9;
                }else{//AMR-NB if this is entered
                    SD_PLAY_offset=6;
                    if(SD_PLAY_desiredFilePos>0) {
                        fseek(SD_PLAY_currentPlayfile,SD_PLAY_desiredFilePos,SEEK_SET);
                        SD_PLAY_desiredFilePos=0;
                    }
                    readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                    SD_PLAY_amrSM=10;
                }
                break;
                
        case 2: if(SD_PLAY_desiredFilePos>0) {
                    fseek(SD_PLAY_currentPlayfile,SD_PLAY_desiredFilePos,SEEK_SET);
                    SD_PLAY_desiredFilePos=0;
                }
                readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                SD_PLAY_amrSM=10;
                break;
       case 10: 
                if(SD_PLAY_blockSize==0){
                    SD_PLAY_blockSize=len+1;
                    //ESP_LOGI(SD_PLAY_LOG_TAG,"AMR: offset %llu blockSize %llu",SD_PLAY_offset,SD_PLAY_blockSize);
                }
                readBytes=fread(buf,1,len,SD_PLAY_currentPlayfile);
                if(readBytes<=0){
                    if(fgetpos(SD_PLAY_currentPlayfile,&fpos)!=0){
                        return AEL_IO_FAIL;
                    }
                    SD_PLAY_currentFilePos=fpos;
                    fclose(SD_PLAY_currentPlayfile);
                    SD_PLAY_currentPlayfile=NULL;
                    SD_PLAY_amrSM=0;
                    return AEL_IO_DONE;
                }
    }
    return readBytes;
}

void SD_PLAY_stopPlaying(){
    if(xSemaphoreTake(SD_PLAY_semStopPlaying,500) == pdTRUE){
        if(SD_PLAY_pipeline!=NULL){

/*            audio_element_stop(SD_PLAY_i2sStreamWriter);
            if(audio_element_wait_for_stop_ms(SD_PLAY_i2sStreamWriter,100)!=ESP_OK){
                ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR Could not stop the I2S writer.");
            }
*/
            if(audio_pipeline_stop(SD_PLAY_pipeline)!=ESP_OK){
                ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR Could not stop the pipeline.");
            }
            if(audio_pipeline_wait_for_stop_with_ticks(SD_PLAY_pipeline,200)!=ESP_OK){
                ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR Timeout when wating for stopping the pipeline.");
            }

            audio_pipeline_unregister(SD_PLAY_pipeline, SD_PLAY_i2sStreamWriter);
            audio_pipeline_unregister(SD_PLAY_pipeline, SD_PLAY_volumeFilter);
            audio_pipeline_unregister(SD_PLAY_pipeline, SD_PLAY_musicFormatDecoder);
            audio_element_deinit(SD_PLAY_musicFormatDecoder);
            SD_PLAY_musicFormatDecoder=NULL;

            audio_event_iface_destroy(SD_PLAY_eventChannel);
            SD_PLAY_eventChannel=NULL;

            audio_pipeline_deinit(SD_PLAY_pipeline);
            SD_PLAY_pipeline=NULL;
            if(SD_PLAY_currentPlayfile!=NULL) fclose(SD_PLAY_currentPlayfile); //close a might be open file if needed
            SD_PLAY_currentPlayfile=NULL;
        }
        xSemaphoreGive(SD_PLAY_semStopPlaying);
        SD_PLAY_internalSM=0;//new state stopped
    }else{
        ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR: Could not stop the current playback");
    }
}

//plays a file over the setup pipeline until being stopped or the file ended
void SD_PLAY_playLoopThread(){
    audio_event_iface_msg_t msg;
    SD_PLAY_message_t sdPlayMsg;

    while(1){
        esp_err_t ret = audio_event_iface_listen(SD_PLAY_eventChannel, &msg, pdMS_TO_TICKS(50));
        if ((ret != ESP_OK)|| ((int)msg.data == AEL_STATUS_ERROR_PROCESS) || ((int)msg.data ==AEL_STATUS_ERROR_INPUT) )  {
            sdPlayMsg.msgType=SD_PLAY_MSG_TYPE_FILEPOS_STATE;
            sdPlayMsg.filePos=SD_PLAY_currentFilePos;
            sdPlayMsg.fileSize=SD_PLAY_currentFileSize;
            sdPlayMsg.blockSize=SD_PLAY_blockSize;
            sdPlayMsg.offset=SD_PLAY_offset;
            sdPlayMsg.bitrate=SD_PLAY_bitrate;
            sdPlayMsg.channels=SD_PLAY_channels;
            audio_element_state_t decoderState=audio_element_get_state(SD_PLAY_musicFormatDecoder);
            //ESP_LOGE(SD_PLAY_LOG_TAG, "[ * ] Event interface error. Decoder state : %d %i", decoderState,(int)msg.data == AEL_STATUS_ERROR_PROCESS);
            if((decoderState>=AEL_STATE_ERROR)|| ((int)msg.data == AEL_STATUS_ERROR_PROCESS)) {
                SD_PLAY_stopPlaying();
                sdPlayMsg.msgType=SD_PLAY_MSG_TYPE_STOPPED_ERROR;
                if(SD_PLAY_sendMessageOut(&sdPlayMsg,100)!=0) ESP_LOGE(SD_PLAY_LOG_TAG, "[ * ] ERROR SENDING QUEUE0");
                break;
            }else{
                if(SD_PLAY_desiredFilePos==0) SD_PLAY_sendMessageOut(&sdPlayMsg,100);//only send state after jump to desired position
            }
            continue;
        }else
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) SD_PLAY_musicFormatDecoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            SD_PLAY_internalSM=2;//new state playing
            SD_PLAY_codecSyned=true;
            audio_element_info_t music_info = {0};
            audio_element_getinfo(SD_PLAY_musicFormatDecoder, &music_info);
            ESP_LOGI(SD_PLAY_LOG_TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d duration=%d",
                    music_info.sample_rates, music_info.bits, music_info.channels,music_info.duration);
            audio_element_info_t i2s_info = {0};
            audio_element_getinfo(SD_PLAY_i2sStreamWriter, &i2s_info);
            if((i2s_info.sample_rates!=music_info.sample_rates)||(i2s_info.bits!=music_info.bits)){
                ESP_LOGE(SD_PLAY_LOG_TAG,"I2S and decoder disagree, choosing decoder %i,%i,%i",music_info.sample_rates, music_info.bits, music_info.channels);
                i2s_stream_set_clk(SD_PLAY_i2sStreamWriter, music_info.sample_rates, music_info.bits, i2s_info.channels);
            }
            continue;
        }else
        // Stop when the last pipeline element (output_stream_writer in this case) receives stop event
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) SD_PLAY_i2sStreamWriter
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))){
            ESP_LOGI(SD_PLAY_LOG_TAG, "[ * ] Stop event received msg.data=%i",(int)msg.data);
            sdPlayMsg.msgType=SD_PLAY_MSG_TYPE_STOPPED_NO_ERROR;
            sdPlayMsg.filePos=SD_PLAY_currentFilePos;
            sdPlayMsg.fileSize=SD_PLAY_currentFileSize;
            sdPlayMsg.blockSize=SD_PLAY_blockSize;
            sdPlayMsg.offset=SD_PLAY_offset;
            sdPlayMsg.bitrate=SD_PLAY_bitrate;
            sdPlayMsg.channels=SD_PLAY_channels;
            SD_PLAY_stopPlaying();
            if(SD_PLAY_sendMessageOut(&sdPlayMsg,100)!=0) ESP_LOGE(SD_PLAY_LOG_TAG, "[ * ] ERROR SENDING QUEUE1");
            ESP_LOGI(SD_PLAY_LOG_TAG, "[ * ] Stop Message send to main");
            break;
        }else{
            //ESP_LOGI(SD_PLAY_LOG_TAG, "UNKONW MESSAGE source:%i cmd:%i type:%i data:%i",msg.source_type,msg.cmd,msg.source_type,(int)msg.data);
        }
    }
    vTaskDelete( NULL );
}

uint8_t SD_PLAY_startPlaying(char* file,uint64_t filePos){
    
    SD_PLAY_stopPlaying();
    
    SD_PLAY_codecSyned=false;
    SD_PLAY_amrSM=0;
    SD_PLAY_mp3SM=0;
    SD_PLAY_oggSM=0;
    SD_PLAY_m4aSM=0;
    SD_PLAY_stereoUpMix=false;
    int32_t sampleRate=0,bits=0,channels=0;
    SD_PLAY_bitrate=0;
    SD_PLAY_channels=0;
    SD_PLAY_waitJump=0;

    //anaylze file and select the correct decoder based on the last 3 letters of the file
    uint16_t length=strlen(file);
    struct stat st;
    if(length<3) return 1;
    if(strncasecmp(&file[length-3],"mp3",3)==0){
        SD_PLAY_currentPlayfile = fopen(file,"r");
        if((SD_PLAY_currentPlayfile)&&(stat(file, &st)==0)){
            SD_PLAY_currentFileSize=st.st_size;
            SD_PLAY_currentFilePos=0;
            if(filePos>SD_PLAY_currentFileSize) filePos=0;
            mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
            mp3_cfg.task_core = 1;
            SD_PLAY_musicFormatDecoder= mp3_decoder_init(&mp3_cfg);
            audio_element_set_read_cb(SD_PLAY_musicFormatDecoder, SD_PLAY_mp3ReadCb, NULL);
            if(FORMAT_HELPER_getMP3FormatInformation(SD_PLAY_currentPlayfile,&sampleRate,&bits,&channels,&SD_PLAY_offset,&SD_PLAY_bitrate,&SD_PLAY_blockSize,SD_PLAY_currentFileSize)==0){
                ESP_LOGI(SD_PLAY_LOG_TAG,"Presetting MP3: %lu,%lu,%lu,%lu",sampleRate,bits,channels,SD_PLAY_bitrate);
                audio_element_set_music_info(SD_PLAY_volumeFilter,sampleRate,channels,bits);
                if(i2s_stream_set_clk(SD_PLAY_i2sStreamWriter, sampleRate, bits, channels)!=ESP_OK){
                    ESP_LOGE(SD_PLAY_LOG_TAG,"Presetting MP3 error.");
                }
            }
        }else{
            return 3;
        }
    }else if((strncasecmp(&file[length-3],"mp4",3)==0)||(strncasecmp(&file[length-3],"m4a",3)==0)||(strncasecmp(&file[length-3],"aac",3)==0)||(strncasecmp(&file[length-3],"m4b",3)==0)){
        SD_PLAY_currentPlayfile = fopen(file,"r");
        if((SD_PLAY_currentPlayfile)&&(stat(file, &st)==0)){
            SD_PLAY_currentFileSize=st.st_size;
            SD_PLAY_currentFilePos=0;
            if(filePos>SD_PLAY_currentFileSize) filePos=0;
            aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
            aac_cfg.task_core = 1;
            SD_PLAY_musicFormatDecoder= aac_decoder_init(&aac_cfg);
            audio_element_set_read_cb(SD_PLAY_musicFormatDecoder, SD_PLAY_m4aReadCb, NULL);
            if(FORMAT_HELPER_getM4AFormatInformation(SD_PLAY_currentPlayfile,&sampleRate,&bits,&channels,&SD_PLAY_offset,&SD_PLAY_bitrate,&SD_PLAY_blockSize,SD_PLAY_currentFileSize)==0){
                ESP_LOGI(SD_PLAY_LOG_TAG,"Presetting M4A: %lu,%lu,%lu",sampleRate,bits,channels);
                audio_element_set_music_info(SD_PLAY_volumeFilter,sampleRate,channels,bits);
                if(i2s_stream_set_clk(SD_PLAY_i2sStreamWriter, sampleRate, bits, channels)!=ESP_OK){
                    ESP_LOGE(SD_PLAY_LOG_TAG,"Presetting M4A error.");
                }
            }
        }else{
            return 3;
        }
    }else if((strncasecmp(&file[length-3],"awb",3)==0)||(strncasecmp(&file[length-3],"amr",3)==0)){
        SD_PLAY_currentPlayfile = fopen(file,"r");
        if((SD_PLAY_currentPlayfile)&&(stat(file, &st)==0)){
            SD_PLAY_currentFileSize=st.st_size;
            if(filePos>SD_PLAY_currentFileSize) filePos=0;
            SD_PLAY_currentFilePos=0;
            amr_decoder_cfg_t amr_cfg = DEFAULT_AMR_DECODER_CONFIG();
            amr_cfg.task_core = 1;
            //amr_cfg.task_prio=uxTaskPriorityGet(NULL)+5;
            SD_PLAY_musicFormatDecoder= amr_decoder_init(&amr_cfg);
            audio_element_set_read_cb(SD_PLAY_musicFormatDecoder, SD_PLAY_amrReadCb, NULL);
            if(FORMAT_HELPER_getAMRFormatInformation(SD_PLAY_currentPlayfile,&sampleRate,&bits,&channels,&SD_PLAY_offset,&SD_PLAY_bitrate,&SD_PLAY_blockSize,SD_PLAY_currentFileSize,&SD_PLAY_stereoUpMix)==0){
                ESP_LOGI(SD_PLAY_LOG_TAG,"Presetting AMR: %lu,%lu,%lu",sampleRate,bits,channels);
                audio_element_set_music_info(SD_PLAY_volumeFilter,sampleRate,channels,bits);
                if(i2s_stream_set_clk(SD_PLAY_i2sStreamWriter, sampleRate, bits, channels)!=ESP_OK){
                    ESP_LOGE(SD_PLAY_LOG_TAG,"Presetting AMR error.");
                }
            }
        }else{
            return 3;
        }
    }else if(strncasecmp(&file[length-3],"ogg",3)==0){
        SD_PLAY_currentPlayfile = fopen(file,"r");
        if((SD_PLAY_currentPlayfile)&&(stat(file, &st)==0)){
            SD_PLAY_currentFileSize=st.st_size;
            if(filePos>SD_PLAY_currentFileSize) filePos=0;
            SD_PLAY_currentFilePos=0;
            ogg_decoder_cfg_t ogg_cfg = DEFAULT_OGG_DECODER_CONFIG();
            ogg_cfg.task_core = 1;
            SD_PLAY_musicFormatDecoder= ogg_decoder_init(&ogg_cfg);
            audio_element_set_read_cb(SD_PLAY_musicFormatDecoder, SD_PLAY_oggReadCb, NULL);
            if(FORMAT_HELPER_getOGGFormatInformation(SD_PLAY_currentPlayfile,&sampleRate,&bits,&channels,&SD_PLAY_offset,&SD_PLAY_bitrate,&SD_PLAY_blockSize,SD_PLAY_currentFileSize)==0){
                ESP_LOGI(SD_PLAY_LOG_TAG,"Presetting OGG: %lu,%lu,%lu",sampleRate,bits,channels);
                audio_element_set_music_info(SD_PLAY_volumeFilter,sampleRate,channels,bits);
                if(i2s_stream_set_clk(SD_PLAY_i2sStreamWriter, sampleRate, bits, channels)!=ESP_OK){
                    ESP_LOGE(SD_PLAY_LOG_TAG,"Presetting OGG error.");
                }
            }
        }else{
            return 3;
        }
    }else{
        return 2;
    }
    SD_PLAY_channels=channels;
    SD_PLAY_desiredFilePos=filePos;
    //pipeline creating
    audio_pipeline_cfg_t pipelineCfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    SD_PLAY_pipeline = audio_pipeline_init(&pipelineCfg);
    mem_assert(SD_PLAY_pipeline);

    audio_pipeline_register(SD_PLAY_pipeline, SD_PLAY_musicFormatDecoder, "mfd");
    audio_pipeline_register(SD_PLAY_pipeline, SD_PLAY_volumeFilter, "volume");
    audio_pipeline_register(SD_PLAY_pipeline, SD_PLAY_i2sStreamWriter, "i2s");

    const char *link_tags[3] = {"mfd", "volume","i2s"};
    audio_pipeline_link(SD_PLAY_pipeline, &link_tags[0], 3);

    audio_event_iface_cfg_t evtCfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    SD_PLAY_eventChannel = audio_event_iface_init(&evtCfg);
    audio_pipeline_set_listener(SD_PLAY_pipeline, SD_PLAY_eventChannel);
    audio_pipeline_run(SD_PLAY_pipeline);

    xTaskCreate(SD_PLAY_playLoopThread, "SD_PLAY_playLoopThread", 1024 * 10, NULL,  uxTaskPriorityGet(NULL), NULL);

    vTaskDelay(50);//we need this here as we need to give the new pipeline time to initialize otherwise a half initalized pipeline that might get stopped by the wich to play a new song will crash the esp
    return 0;

}
void SD_PLAY_messageLoopThread(){
    SD_PLAY_message_t msg;
    while(1){
        if(xQueueReceive(SD_PLAY_inQueue,&msg,pdMS_TO_TICKS(100)) == pdPASS ){//wait for incoming messages
            if((msg.msgType==SD_PLAY_MSG_TYPE_START_PLAY)&&(SD_PLAY_internalSM==0)){
                SD_PLAY_internalSM=1;
                if(SD_PLAY_startPlaying(msg.msgData,msg.filePos)!=0){
                    SD_PLAY_internalSM=0;
                    msg.msgType=SD_PLAY_MSG_TYPE_STOPPED_ERROR;
                    SD_PLAY_sendMessageOut(&msg,100);
                }
            }else if((msg.msgType==SD_PLAY_MSG_TYPE_STOP_PLAY)&&(SD_PLAY_internalSM==2)){
                SD_PLAY_internalSM=3;
                SD_PLAY_stopPlaying();
            }
        }
    }
}

uint8_t SD_PLAY_startService(){
    //init i2s port
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    SD_PLAY_i2sStreamWriter = i2s_stream_init(&i2s_cfg);

    //create volume filter audio element
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.process = SD_PLAY_volumeFilterProcess;
    cfg.open =SD_PLAY_volumeFilterOpen;
    cfg.close = SD_PLAY_volumeFilterClose;
    cfg.destroy = SD_PLAY_volumeFilterDestroy;
    cfg.buffer_len = SD_PLAY_VOLUMEFILTER_BUFFER_SIZE;
    cfg.tag = "volume";
    cfg.task_core = 0;
    cfg.task_stack =6*1024;
    SD_PLAY_volumeFilter = audio_element_init(&cfg);
    
    SD_PLAY_semStopPlaying=xSemaphoreCreateBinary();
    if(SD_PLAY_semStopPlaying==NULL){
        ESP_LOGE(SD_PLAY_LOG_TAG,"ERROR: Could not create semStopPlaying semaphore.");
    }else{
        xSemaphoreGive(SD_PLAY_semStopPlaying);
    }

    SD_PLAY_inQueue=xQueueCreate(1, sizeof(SD_PLAY_message_t));
    SD_PLAY_outQueue=xQueueCreate(10, sizeof(SD_PLAY_message_t));

    xTaskCreate(SD_PLAY_messageLoopThread, "SD_PLAY_messageLoopThread", 1024 * 10, NULL,  uxTaskPriorityGet(NULL), NULL);


    return 0;
}

uint8_t SD_PLAY_sendMessage(SD_PLAY_message_t* msg, uint16_t waitTime){
    if(SD_PLAY_inQueue==NULL) return 1;
    if(xQueueOverwrite(SD_PLAY_inQueue,msg)!=pdPASS){
        return 2;
    }
    return 0;
}


uint8_t SD_PLAY_getMessage(SD_PLAY_message_t* msg, uint16_t waitTime){
    if(SD_PLAY_outQueue==NULL) return 1;
    memset(msg,0,sizeof(SD_PLAY_message_t));
    if(xQueueReceive(SD_PLAY_outQueue,msg,pdMS_TO_TICKS(waitTime)) != pdPASS ){
        return 2;
    }
    return 0;
}

#endif
