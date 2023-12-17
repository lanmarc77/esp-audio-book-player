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
#ifndef SAVES_C
#define SAVES_C
#include <string.h>
#include <mbedtls/md5.h>
#include "saves.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "sd_card.h"

#define SAVES_LOG_TAG "SAVES"

esp_vfs_spiffs_conf_t SAVES_spiffsConf;

void SAVES_deInit(){
    esp_vfs_spiffs_unregister(SAVES_spiffsConf.partition_label);
}

void SAVES_spiffsFormat(){
    esp_spiffs_format(NULL);
}

void SAVES_init(){

    ESP_LOGI(SAVES_LOG_TAG, "Initializing SPIFFS");

    SAVES_spiffsConf.base_path = "/s";
    SAVES_spiffsConf.partition_label = NULL;
    SAVES_spiffsConf.max_files = 10;
    SAVES_spiffsConf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(&SAVES_spiffsConf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SAVES_LOG_TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(SAVES_LOG_TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(SAVES_LOG_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SAVES_spiffsConf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(SAVES_LOG_TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(SAVES_spiffsConf.partition_label);
        return;
    } else {
        ESP_LOGI(SAVES_LOG_TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partiton size info.
    if (used > total) {
        ESP_LOGW(SAVES_LOG_TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(SAVES_spiffsConf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(SAVES_LOG_TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGI(SAVES_LOG_TAG, "SPIFFS_check() successful");
        }
    }
}

uint8_t SAVES_loadSettings(SAVES_settings_t *settings){
    FILE* f = fopen("/s/settings.cfg", "rb");
    if (f == NULL) {
        return 1;
    }
    int32_t result=fread(settings,1,sizeof(SAVES_settings_t),f);
    if(result>=sizeof(SAVES_settings_t)){
        fclose(f);
        return 0;
    }else{
        ESP_LOGE(SAVES_LOG_TAG,"Settings restore error. Result %li",result);
    }
    fclose(f);
    return 1;
}

uint8_t SAVES_saveSettings(SAVES_settings_t *settings){
    FILE* f = fopen("/s/settings.cfg", "wb");
    settings->version=0;
    if (f == NULL) {
        ESP_LOGE(SAVES_LOG_TAG,"Could not open settings.cfg for writing.");
        return 1;
    }
    int32_t result=fwrite(settings,1,sizeof(SAVES_settings_t),f);
    if(result==sizeof(SAVES_settings_t)){
        fclose(f);
        return 0;
    }else{
        ESP_LOGE(SAVES_LOG_TAG,"Settings save error. Result %li",result);
    }
    fclose(f);
    return 1;
}

void SAVES_getBookmarkFileFromFolderName(char* folderName,char* saveFileName){
    mbedtls_md5_context md5Ctx;
    unsigned char md5Hash[16];
    mbedtls_md5_init(&md5Ctx);
    for(uint16_t i=0;i<strlen(folderName);i++){
        unsigned char v=folderName[i];
        mbedtls_md5_update(&md5Ctx,&v,1);
    }
    mbedtls_md5_finish(&md5Ctx,&md5Hash[0]);
    mbedtls_md5_free(&md5Ctx);
    sprintf(saveFileName,"/s/%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X.b",md5Hash[0],md5Hash[1],md5Hash[2],md5Hash[3],md5Hash[4],md5Hash[5],md5Hash[6],md5Hash[7],md5Hash[8],md5Hash[9],md5Hash[10],md5Hash[11],md5Hash[12],md5Hash[13],md5Hash[14],md5Hash[15]);
}

uint8_t SAVES_existsBookmark(char* folderName,SAVES_saveState_t* save){
    char saveFileName[64];
    SAVES_getBookmarkFileFromFolderName(folderName,&saveFileName[0]);
    //ESP_LOGI(SAVES_LOG_TAG,"Searching for bookmark file %s",&saveFileName[0]);
    FILE* f = fopen(&saveFileName[0], "rb");
    if (f == NULL) {
        return 1;
    }
    int32_t result=fread(save,1,sizeof(SAVES_saveState_t),f);
    if(result>=sizeof(SAVES_saveState_t)){
        ESP_LOGI(SAVES_LOG_TAG,"Restored ok. folder \"%s\" file \"%s\" ",save->folderName,save->fileName);
        fclose(f);
        return 0;
    }else{
        ESP_LOGE(SAVES_LOG_TAG,"Restore error. Result %li",result);
    }
    fclose(f);
    return 1;
}

uint8_t SAVES_saveBookmark(char* folderName,SAVES_saveState_t* save){
    char saveFileName[64];
    SAVES_getBookmarkFileFromFolderName(folderName,&saveFileName[0]);
    //ESP_LOGI(SAVES_LOG_TAG,"Saving for bookmark file %s at position %llu for playfile %s",&saveFileName[0],save->playPosition,save->fileName);
    FILE* f = fopen(&saveFileName[0], "wb");
    save->version=0;
    if (f == NULL) {
        ESP_LOGE(SAVES_LOG_TAG,"Could not open bookmark for writing.");
        return 1;
    }
    strcpy(&save->folderName[0],folderName);
    int32_t result=fwrite(save,1,sizeof(SAVES_saveState_t),f);
    if(result==sizeof(SAVES_saveState_t)){
        //ESP_LOGI(SAVES_LOG_TAG,"Saved ok");
        fclose(f);
        return 0;
    }else{
        ESP_LOGE(SAVES_LOG_TAG,"Save error. Result %li",result);
    }
    fclose(f);
    return 1;
}

//deletes bookmarks from the spiffs which folders+files do not exist on the sd card (anymore)
void SAVES_cleanOldBookmarks(){
    char fullfile[FF_FILE_PATH_MAX];
    char fullfile2[FF_FILE_PATH_MAX];
    struct dirent *currentEntry;
    SAVES_saveState_t save;
    FILE* f;
    FILE* f2;
    DIR *dir = opendir("/s");
    if(dir){
        while ((currentEntry = readdir(dir)) != NULL) {
            if(currentEntry->d_type!=DT_DIR){
                if(currentEntry->d_name[strlen(currentEntry->d_name)-1]=='b'){
                    strcpy(&fullfile[0],"/s/");
                    strcat(&fullfile[0],currentEntry->d_name);
                    f = fopen(&fullfile[0], "rb");
                    if (f == NULL) {
                        ESP_LOGE(SAVES_LOG_TAG,"Could not open save file %s from SPIFFS.",&fullfile[0]);
                        continue;//ignore files that cant be opened
                    }
                    int32_t result=fread(&save,1,sizeof(SAVES_saveState_t),f);
                    if(result>=sizeof(SAVES_saveState_t)){
                        strcpy(&fullfile2[0],SD_CARD_MOUNT_POINT"/");
                        strcat(&fullfile2[0],&save.folderName[0]);
                        strcat(&fullfile2[0],"/");
                        strcat(&fullfile2[0],&save.fileName[0]);
                        f2 = fopen(&fullfile2[0], "rb");
                        if(f2==NULL){
                            if(remove(&fullfile[0])==0){
                                ESP_LOGI(SAVES_LOG_TAG,"Deleted missing save: %s->%s",&fullfile[0],&fullfile2[0]);
                            }else{
                                ESP_LOGE(SAVES_LOG_TAG,"ERROR deleting missing save: %s->%s",&fullfile[0],&fullfile2[0]);
                            }
                        }
                        fclose(f2);
                    }
                    fclose(f);
                }
            }
        }
        closedir(dir);
    }else{
        closedir(dir);
    }
}

#endif
