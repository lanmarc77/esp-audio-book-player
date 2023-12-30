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
#ifndef FF_HANDLING_C
#define FF_HANDLING_C
#include <string.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "ff_handling.h"
#include "sd_card.h"

#define FF_LOG_TAG "FF"

/**
  * @brief  gets a sorted list of files or folders inside a given path
  *
  * @param[in] folderPath pointer to full vfs mounted path
  * @param[out] amountOfEntries pointer which holds the number of sorted entries
  * @param[out] sortedArry pointer to an an array which contains the IDs of the found files sorted ascending
  * @param[in] ffType which type of entries to look for: 0=directory, 1=files
  * @param[in] outQueue pointer to a queue handle where the current status of the finding/sorting can be reported for the UI
  * @param[in] inQueue pointer to queue handle to stop the sorting process from the UI, send -1 to request a stop
  * @param[in] searchString pointer to a string of one specific entrystring (usually a filename) whichs sorting position is to be reported in searchId
  * @param[out] searchId pointer to a value which will hold the sorting postion of the file to be searched via searchString
  * @param[out] flags bit0=repeat.dat found in folder, bit1: autostart.dat found in folder
  * 
  * @return 1=ok, finished (-1 is sent to outQueue), 2=folderPath not found (-2 is sent to outQueue)
  *         3=to many entries (-3 is sent to outQueue), 4=canceld sorting/finding as requested (-4 is sent to outQueue)
  */
uint8_t FF_getList(char* folderPath,uint16_t* amountOfEntries,uint16_t* sortedIdArray,uint8_t ffType,QueueHandle_t* outQueue, QueueHandle_t* inQueue, char* searchString,int32_t* searchId,uint8_t* flags){
    uint16_t maxIdCounter=FF_MAX_SORT_ELEMENTS;//limits the files to sort even if more files exist
    struct dirent *currentEntry;
    DIR *dir = opendir(folderPath);
    char minLastName[FF_FILE_PATH_MAX];
    char minNewCandidateName[FF_FILE_PATH_MAX];
    uint16_t sortedPosition=0;
    uint16_t maxFiles=0;
    minLastName[0]=0;
    minNewCandidateName[0]=0;
    uint64_t startTime=esp_timer_get_time();
    *amountOfEntries=0;
    int32_t outQueueMsg=0;
    int32_t inQueueMsg=0;
    ESP_LOGI(FF_LOG_TAG,"Scanning: %s",folderPath);
    if(dir){
        memset(&sortedIdArray[0],0,FF_MAX_SORT_ELEMENTS*2);
            
        do{
            uint16_t minNewCandidateID=0,positionCounter=0;
            minNewCandidateName[0]=0;
            while (((currentEntry = readdir(dir)) != NULL)&&(positionCounter<maxIdCounter)) {
                if((ffType==0)&&(currentEntry->d_type!=DT_DIR)){//directory needs to be directory type
                    continue;
                }else if((ffType==1)&&(currentEntry->d_type==DT_DIR)){//file is not allowed to have directory type
                    continue;
                }
                if(ffType==0){
                    if(strcmp(&currentEntry->d_name[0],FW_DIR_NAME)==0){//ignore fwupgrade directory
                        continue;
                    }
                }
                if(ffType==1){//check valid file extensions
                    uint16_t l=strlen(currentEntry->d_name);
                    if(strcmp(&currentEntry->d_name[0],"repeat.dat")==0){
                        if(flags!=NULL){
                            *flags|=FF_REPEAT_FLAG;
                        }
                    }
                    if(strcmp(&currentEntry->d_name[0],"autostart.dat")==0){
                        if(flags!=NULL){
                            *flags|=FF_AUTOSTART_FLAG;
                        }
                    }
                    if(!((strncasecmp(&currentEntry->d_name[l-3],"mp3",3)==0) ||
                         (strncasecmp(&currentEntry->d_name[l-3],"awb",3)==0) ||
                         (strncasecmp(&currentEntry->d_name[l-3],"amr",3)==0) ||
                         /*(strncasecmp(&currentEntry->d_name[l-3],"m4a",3)==0) ||
                         (strncasecmp(&currentEntry->d_name[l-3],"aac",3)==0) ||
                         (strncasecmp(&currentEntry->d_name[l-3],"m4b",3)==0) ||*/
                         (strncasecmp(&currentEntry->d_name[l-3],"ogg",3)==0)))
                    {
                        continue;
                    }
                }
                if(strcmp(currentEntry->d_name,&minLastName[0])>0){//it is only a new candidate if it is greater then the last minimum
                    if(minNewCandidateName[0]==0){
                        strcpy(&minNewCandidateName[0],currentEntry->d_name);
                        minNewCandidateID=positionCounter+1;
                    }
                    if(strcmp(currentEntry->d_name,&minNewCandidateName[0])<0){//the current entry is smaller, make it the new candidate
                        strcpy(&minNewCandidateName[0],currentEntry->d_name);
                        minNewCandidateID=positionCounter+1;
                    }
                }
                positionCounter++;
                if(positionCounter>=maxIdCounter){
                    outQueueMsg=-3;
                    closedir(dir);
                    xQueueOverwrite(*outQueue,&outQueueMsg);
                    return 3;
                }
                
                if(xQueueReceive(*inQueue,&inQueueMsg,0)== pdPASS){
                    if(inQueueMsg==-1){
                        outQueueMsg=-4;
                        closedir(dir);
                        xQueueOverwrite(*outQueue,&outQueueMsg);
                        return 4;
                    }
                }
            }
            maxFiles=positionCounter;
            *amountOfEntries=sortedPosition;
            if(minNewCandidateName[0]!=0){
                rewinddir(dir);
                strcpy(&minLastName[0],&minNewCandidateName[0]);

                double percentDone=(100.0*sortedPosition)/maxFiles;
                outQueueMsg=(int32_t)(percentDone*10);
                xQueueOverwrite(*outQueue,&outQueueMsg);
                //uint64_t timeDiffS=(esp_timer_get_time()-startTime)/1000000;
                //uint64_t timeLeftS=((100*timeDiffS)/percentDone)-timeDiffS;
                //ESP_LOGI(FF_LOG_TAG,"Sort: %s  (%d)  %i/%i  %.1f%%  %llu:%02llu",&minNewCandidateName[0],minNewCandidateID,sortedPosition,maxFiles,percentDone,timeLeftS/60,timeLeftS%60);
                if((searchString!=NULL)&&(searchString[0]!=0)){
                    if(strcasecmp(&minNewCandidateName[0],&searchString[0])==0){
                        *searchId=sortedPosition;
                    }
                }
                sortedIdArray[sortedPosition++]=minNewCandidateID;
            }
        }while(minNewCandidateName[0]!=0);
        closedir(dir);
    }else{
        outQueueMsg=-2;
        xQueueOverwrite(*outQueue,&outQueueMsg);
        return 2;
    }
    outQueueMsg=-1;
    xQueueOverwrite(*outQueue,&outQueueMsg);
    return 1;
}

/**
  * @brief  gets a file/foldername inside a path by a given ID (position inside FAT)
  *
  * @param[in] folderBasePath pointer to full vfs mounted path
  * @param[in] ID ID of file
  * @param[out] resultName pointer to a char array where the name will be stored
  * @param[in] ffType which type of entries to look for: 0=directory, 1=files
  * 
  * @return 0=ok, file found, 1=folderBasePath not found, 2=file not found
  */
uint8_t FF_getNameByID(char* folderBasePath,uint16_t ID,char *resultName,uint8_t ffType){
    struct dirent *currentEntry;
    /*if(folderBasePath[strlen(folderBasePath)-1]!='/'){
        strcat(folderBasePath,"/");
    }*/
    DIR *dir = opendir(folderBasePath);
    uint16_t idCounter=0;
    if(dir){
        while ((currentEntry = readdir(dir)) != NULL) {
            if((ffType==0)&&(currentEntry->d_type!=DT_DIR)){//directory needs to be directory type
                continue;
            }else if((ffType==1)&&(currentEntry->d_type==DT_DIR)){//file is not allowed to have directory type
                continue;
            }
            if(ffType==1){//check valid file extensions
                uint16_t l=strlen(currentEntry->d_name);
                if(!((strncasecmp(&currentEntry->d_name[l-3],"mp3",3)==0) ||
                        (strncasecmp(&currentEntry->d_name[l-3],"awb",3)==0) ||
                        (strncasecmp(&currentEntry->d_name[l-3],"amr",3)==0) ||
                        /*(strncasecmp(&currentEntry->d_name[l-3],"m4a",3)==0) ||
                        (strncasecmp(&currentEntry->d_name[l-3],"aac",3)==0) ||
                        (strncasecmp(&currentEntry->d_name[l-3],"m4b",3)==0) ||*/
                        (strncasecmp(&currentEntry->d_name[l-3],"ogg",3)==0)))
                {
                    continue;
                }
            }
            if(idCounter+1==ID){
                strcpy(resultName,currentEntry->d_name);
                closedir(dir);
                return 0;
            }
            idCounter++;
        }
        closedir(dir);
    }else{
        return 1;
    }
    return 2;
}

#endif
