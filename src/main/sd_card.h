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
#ifndef SD_CARD_H
#define SD_CARD_H

/*
    Handles low level SD card handling like bus initialization and mounting into esp VFS
*/

#define SD_CARD_USE_SD_MODE
#define SD_CARD_MOUNT_POINT "/a"
#define FW_DIR_NAME "fwupgrade"
#define FW_DIR_PATH SD_CARD_MOUNT_POINT"/"FW_DIR_NAME

//connector DO | DAT0
#define SD_CARD_PIN_NUM_MISO 33
//connector DI | CMD
#define SD_CARD_PIN_NUM_MOSI 48
//connector CLK
#define SD_CARD_PIN_NUM_CLK  47
//connector CS | DAT3
#define SD_CARD_PIN_NUM_CS   26

//connector DO | DAT0 | MISO
#define SD_CARD_PIN_D0 33
//connector CLK | SCK
#define SD_CARD_PIN_CLK 47
//connector DI | CMD | MOSI
#define SD_CARD_PIN_CMD 48
//connector CS | DAT3
#define SD_CARD_PIN_D3 26


//initalizes SD card bus and ounts the card from esp vfs
uint8_t SD_CARD_init();
//deinitalizes SD card bus and umounts the card from esp vfs
uint8_t SD_CARD_deInit();

#endif
