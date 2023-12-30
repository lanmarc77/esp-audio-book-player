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
#ifndef SD_CARD_C
#define SD_CARD_C
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "sd_card.h"

#define SD_CARD_LOG_TAG "SD_CARD"
//0=SD card was not yet initialized, 1=SD card bus was initalized, 2=SD bus was initialized and card was mounted
uint8_t SD_CARD_sdState=0;
sdmmc_host_t SD_CARD_host = SDSPI_HOST_DEFAULT();
const char SD_CARD_mount_point[] = SD_CARD_MOUNT_POINT;
sdmmc_card_t *SD_CARD_card;

/**
  * @brief initializes and mounts the SD card into the VFS
  *
  * @return 0=ok, SD card mounted, 1=error, SD card not mounted
  * 
  */
uint8_t SD_CARD_init(){
    esp_err_t ret;
    if(SD_CARD_sdState==2){
#ifdef SD_CARD_USE_SD_MODE
        esp_vfs_fat_sdcard_unmount(SD_CARD_mount_point, SD_CARD_card);
#else
        // All done, unmount partition and disable SPI peripheral
        esp_vfs_fat_sdcard_unmount(SD_CARD_mount_point, SD_CARD_card);
        //deinitialize the bus after all devices are removed
        spi_bus_free(SD_CARD_host.slot);
#endif
        ESP_LOGI(SD_CARD_LOG_TAG, "Card unmounted");
    }else if(SD_CARD_sdState==1){
#ifdef USE_SD_MODE
#else
        //deinitialize the bus after all devices are removed
        spi_bus_free(SD_CARD_host.slot);
#endif
    }

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    ESP_LOGI(SD_CARD_LOG_TAG, "Initializing SD card");
    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.


#ifdef SD_CARD_USE_SD_MODE

    esp_rom_gpio_pad_select_gpio(SD_CARD_PIN_D3);
    gpio_set_pull_mode(SD_CARD_PIN_D3, GPIO_PULLUP_ONLY);
    gpio_set_direction(SD_CARD_PIN_D3, GPIO_MODE_INPUT);

    ESP_LOGI(SD_CARD_LOG_TAG, "Using SDMMC mode peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_CARD_PIN_CLK;
    slot_config.cmd = SD_CARD_PIN_CMD;
    slot_config.d0 = SD_CARD_PIN_D0;

    // Enable internal pullups on enabled pins. Works on short cables only.
    // Maybe external pullups are needed
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    SD_CARD_sdState=1;

    ESP_LOGI(SD_CARD_LOG_TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(SD_CARD_mount_point, &host, &slot_config, &mount_config, &SD_CARD_card);

#else
    ESP_LOGI(SD_CARD_LOG_TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CARD_PIN_NUM_MOSI,
        .miso_io_num = SD_CARD_PIN_NUM_MISO,
        .sclk_io_num = SD_CARD_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024*4,
    };
    ret = spi_bus_initialize(SD_CARD_host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        if(SD_CARD_sdState==0){
            ESP_LOGE(SD_CARD_LOG_TAG, "Failed to initialize bus.");
            vTaskDelay(50);
            return 1;
        }
    }
    SD_CARD_sdState=1;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SD_CARD_host.slot;

    ESP_LOGI(SD_CARD_LOG_TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(SD_CARD_mount_point, &SD_CARD_host, &slot_config, &mount_config, &SD_CARD_card);
#endif

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_CARD_LOG_TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(SD_CARD_LOG_TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        vTaskDelay(50);
        return 1;
    }
    ESP_LOGI(SD_CARD_LOG_TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, SD_CARD_card);

    SD_CARD_sdState=2;
    return 0;
}

#endif

/**
  * @brief deinitializes and ummounts the SD card and releases SPI bus
  *
  * @return 0=ok
  * 
  */
uint8_t SD_CARD_deInit(){
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(SD_CARD_mount_point, SD_CARD_card);
    ESP_LOGI(SD_CARD_LOG_TAG, "Card unmounted");
    
    #ifdef SD_CARD_USE_SD_MODE
    #else
    //deinitialize the bus after all devices are removed
    spi_bus_free(SD_CARD_host.slot);
    #endif
    return 0;
}
