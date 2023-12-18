# An ESP32 based audio book player build from existing electronics modules
**THIS REPO IS WIP. FILES/INFORMATION ARE ADDED STEP BY STEP**

A device that is meant to play non-DRM audio books from an SD card and has the following features:  

- folder based audio book player, one folder contains all files of one audio book
- multiple audio books supported
- sorting of audio book names and audio book files by ASCII name (see limitations of sorting below)
- saves audio book listening position and resumes
- supports huge single file audio books, e.g. a 20hrs long singe file audio book
- file formats AMR-WB, MP3 and OGG
- ASCII character based menu driven UI for 16x4 character screen
- single rotary knob navigation
- sleep timer 1...99min
- auto power off
- firmware upgrades from SD card
- based on pure esp-idf + esp-adf
- plays 30hrs+ with 2500mAh 18650 battery
- endless options of integrations with speakers/cases
  
It tries to follow the design principle of
[doing one thing and doing that well](https://en.wikipedia.org/wiki/Unix_philosophy).
  
# The working prototypes
development board:
![image](pictures/prototype_front.jpg)  
  
integrated in an old speaker that otherwise might have landed in the trash:  
![image](pictures/speaker_integrated.jpg)  
  
TODO: the portable/smaller version:
![image](pictures/mobile_version.jpg)  

[first poc](pictures/first_poc.jpg)
  
# Why?
Currently (2023) one really needs to search hard to find an offline only audio book player that is able to play
files from an SD card and at the same time sorts the files based on their name and remembers and resumes
the listening position.  
Many people will use subscription services with the apps on their smartphone. These services and the apps
can be canceled at any time. May it be politics or economic reasons. They usually do not allow you
own the audio book but only sell a license to listen at it for as long as the company exists or is
allowed to do business with you.  
I have an audio book collection that I created by only buying if I can download the files or
copy them e.g. from a CD to keep them.  
All my audio books are stored in AMR-WB as for me the quality with 24kBit/s is good enough. This shrinks an MP3
file to approx. 1/10ths when using AMR-WB and also makes the backup cheap. One CD with AMR-WB can hold
60hrs, a DVD 350hrs, a 25GB BD-R 2000hrs+.  
Using existing electronics hardware modules and the software of this repository I can now build a device that
just plays my audio books. Not more, not less. And it most likely will do so in 30years assuming nothing breaks.
There are still 30yrs+ old Nintendo Gameboys that work like a charm and are being refurbished.  
Additionally I can change the behavior of the software as I like. If necessary I can install the whole
development environment in an offline virtual machine and will still be able to compile even if the
hardware manufacturer of the main controller board does not exist anymore.  
  
# Videos
Download and watch:  
[Scenario1](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario1.mp4): normal operation  
[Scenario2](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario2.mp4): seeking  
[Scenario3](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario3.mp4): sorting  
[Scenario4](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario4.mp4): auto bookmarks  
[Scenario5](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario5.mp4): sleep timer  
  
# Hardware
The build only needs a few components that are manufactured by companies in mass and therefore cheap.  
For PCB based components the schematics are available in the module datasheets folder.  
  
**Main controller board:**  
The main controller board is a Heltec WiFi Kit 32 V3. It was chosen because it has a very small deep sleep
current of about 10µA. I measured 15µA still very good. It already comes with the circuitry to connect
and charge a rechargeable battery as well as the possibility to disconnect other components from power
by using a software programmable power pin (Ve).  
Finally an OLED display is also already available for the UI.  
Be aware that you buy V3 of this board version as it has the low deep sleep current. Otherwise your battery
looses way too much energy even if you do not use the player. Additionally verify that you also get a fitting
battery cable.  
![image](pictures/heltec_wifi_kit32_v3.jpg)  
Typical price in Germany incl. tax (2023): 20€. 
  
**I²S digital analog audio converter**  
Currently only a MAX98357A based converter has been tested extensively. Also an UDA1334A was tested and it worked.
The MAX98357A already contains an amplifier to directly connect it to a speaker and get a usable volume out
of it.
![image](pictures/MAX98357A.jpg)  
Typical price in Germany incl. tax (2023): 8€. 
  
**SD card holder**  
I still had old big SD cards laying around so I started with a big SD card holder. They also exists in
micro sd card format. I used this model as it comes with the needed pullup resistors already on the PCB.
Additionally I removed the voltage regulator that is used to convert 5V to 3.3V as I drive the board already
with 3.3V and it consumed around 2mA if back fed. I tested SD cards up to 256GB with this board and they worked.  
![image](pictures/sd_card_holder.jpg)  
Typical price in Germany incl. tax (2023): 1.5€. 
  
**Battery and holder**  
Any LiIon battery can be used. I tried 18650 and 14500 without any problems. I suggest trusted dealers as
there are many cheap fake batteries.  
![image](pictures/battery_and_holder.jpg)  
Typical price in Germany for both incl. tax (2023): 8€. 
  
**Rotary knob**  
To minimize wiring and drilling work I decided for a single rotary knob as main navigation option. This model
has an integrated switch. I suggest to buy a version that comes with the washer/nut/cap.  
![image](pictures/knob.png)  
Typical price in Germany incl. tax (2023): 1€. 
  
**PCB**  
The prototypes as seen above use half or full size euro card PCBs. Any will do for manual wiring. You would
use the full size if you need the extra space for mounting it inside e.g. a speaker.  
![image](pictures/breadboard.png)  
Typical price in Germany incl. tax (2023): 2€. 
  
## Wiring diagram
Click on the picture for zoom.  
![image](pictures/wiring.jpg)  
  
The original .odt file of this drawing is in the `src`directory.  
  
# UI navigation
TODO
  
# Uploading initial binary release
For a brand new main controller board an initial upload is needed. This initial upload will setup the correct
partition details and installs the firmware for the first time. After this initial upload firmware upgrades
via the SD card are possible.  
The binary release files in the [release](https://github.com/lanmarc77/esp-audio-book-player/releases) section:  
  
- bootloader.bin
- partition-table.bin
- ota_data_initial.bin
- esp32_player.bin
  
are to be used for this initial upload.  

## Linux and Windows
For Linux and Windows the Espressif esptool.py can be used. It can be installed by following [this guide](https://docs.espressif.com/projects/esptool/en/latest/esp32/).  
A Windows Comport driver can be found in the `src` directory.  
  
The command line for the initial upload should look like this:  
`
esptool.py -p [YOUR PORT e.g. COM7 or /dev/ttyUSB0] -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x10000 esp32_player.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin
`  
  
Since v00.00.03 firmware upgrades are possible via SD card if this initial upload was done.  
  
## Windows using the Flash Download Tools
Espressif also offers the Windows only Flash Download Tools package with a graphical user interface.  
It can be downloaded from [here](https://www.espressif.com/en/support/download/other-tools).  
A Windows Comport driver can be found in the `src` directory.  
  
The following settings need to be made for the initial upload:  
![image](pictures/ft_initial.png)  
Select the correct COM port after making the settings as seen below and press START:  
![image](pictures/ft_main.png)  
Ideally no errors are shown and the initial upload is finished.  
  
# FW upgrade using SD card (incl. rollback support)
For upgrading firmware once the initial upload was done only one specific file from the
[release section](https://github.com/lanmarc77/esp-audio-book-player/releases) is needed: XX.XX.XX_ota.fw  
XX are letters from A-F or/and numbers from 0-9 specifying the firmware version of this release.  
Place the file on the SD card in a folder called `fwupgrade`. Once you start the device it should
present you with the upgrade screen. The upgrade screen is left without upgrading if no user interaction is
happening for 10s.  
If you wish to upgrade shortly press the knob button . A countdown of 10s gives you the possibility
to stop the upgrade if you press the knob button. Once the upgrade started you can not interrupt it.
The actual writing process for the upgrade takes around 10s and the progress is shown.  
  
If everything worked you should now see the device restart with the new firmware version but it displays
"TEST" instead of "ok" in the initial firmware version screen during startup.  
"TEST" means that the new firmware is not yet persisted. In this test state you have the possibility
to test everything and check if the new version suits your needs.  
You can persist the new firmware by manually switching the device off by long pressing the knob button
in the audio book selection screen.  
If you do not want to keep the new version and rollback you need to wait for the
device to auto power off which usually happens after 1 minute with no user interaction. Once auto powered off and
afterwards started again the old version should be activated again.  
  
# Compilation hints
Currently used espressif sdk versions:  
esp-idf:  v5.1.1/commit e088c3766ba440e72268b458a68f27b6e7d63986  
esp-adf:  v2.6/commit 49f80aafefc31642ea98db78bf024e18688b8de9  
  
Needed for exFAT:   
To enable exFAT file esp-idf/components/fatfs/src/ffconf.h needs to edited and macro FF_FS_EXFAT needs to be set 1.  
  
Optionally for more privacy:  
The esp-adf unnecessarily links the nghttp component which on every compilation checks with the servers for updates.
To disable delete file idf_components.yml in components/esp-adf-libs/ and change the COMPONENT_REQUIRES in CMakeLists.txt
so that no nghttp is present anymore. Now complete offline compilation is possible.  
  
Take a look into the `src/patches/` directory for more details and .patch files to apply to above sdk versions.

# Limitations/won't do's
TODO

- sorting, folders, files
- bookmarks storage, number of bookmarks, manual bookmarks
- m4b, drm
- m3u/playlists
- mp3/ogg chapter support
- mp3 cbr/vbr
- charsets other than ascii
- bluetooth
- playing speed
- equalizer
- upload on SD card not via ESP USB connector
  
# Technical discussions
TODO:

- On the board the ESP32-S3 and it's alternative options
- On sorting
- On bookmark storage and cleanup
- On battery life
- On MP3 VBR/CBR
  
# Ideas/future
- multi user support
- audio navigation for handicapped people
- PCB with fitting 3D printable case or fitting for a cheap buyable case

