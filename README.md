# TINAMP: An ESP32 based audio book player build from existing electronics modules
TINAMP: **T**his **I**s **N**ot **A** **M**usic **P**layer.  
  
This is a device that is meant to play non-DRM audio books from an SD card and has the following features:  

- folder based audio book player, one folder contains all files of one audio book
- multiple audio books supported (currently max. 9999 on one SD card)
- sorting of audio book names and audio book files by ASCII name (see limitations of sorting below)
- saves audio book listening position and resumes for each audio book
- supports huge single file audio books, e.g. a 20hrs long singe file audio book
- file formats AMR-WB, MP3 and OGG
- ASCII character based menu driven UI for 16x4 character screen
- single rotary encoder navigation
- sleep timer 1...99min, wakeup timer 1min...24hr
- play speed settings from 0.5...2.5 in 0.05 steps (saved individually for each audio book)
- audio equalizer (more/better presets are WIP)
- auto power off
- firmware upgrades from SD card
- based on pure esp-idf + esp-adf
- plays 50hrs+ with 2500mAh 18650 battery
- charging via USB-C
- endless options of integrations with speakers/cases
  
It tries to follow the design principle of
[doing one thing and doing that well](https://en.wikipedia.org/wiki/Unix_philosophy).
  
# The working prototypes
development board:
![image](pictures/prototype_front.jpg)  
  
integrated in an old speaker that otherwise might have landed in the trash:  
![image](pictures/speaker_integrated.jpg)  
  
the portable/smaller prototype version:  
![image](pictures/mobile_version.jpg)  
  
[first POC (proof of concept)](pictures/first_poc.jpg)
  
# Why?
Currently (2023) one really needs to search hard to find an offline only non tracking audio book player that is
able to play files from huge SD cards and at the same time sorts the files based on their name and remembers and
resumes the last listening position.  
Many people nowadays will use subscription services with corresponding apps on their smartphone. These services
and the apps usually track the users. They can also be canceled at any time. May it be for politics or economic
reasons. They usually do not allow you to own the audio book but only sell a license to listen at it for as long
as the company exists or is allowed to do business with you.  
I have an audio book collection that I created by only buying if I can download the files or
copy them e.g. from a CD to keep them. Without DRM.  
All my audio books are converted to and then stored in AMR-WB as for me the quality with 24kBit/s mono is good enough.
This shrinks an MP3 file to approx. 1/10ths when using AMR-WB and also makes the backup cheap. One CD with AMR-WB
can hold 60hrs, a DVD 350hrs, a 25GB BD-R 2000hrs+.  
Using existing electronics hardware modules and the software of this repository I can now build a device that
just plays my audio books. Not more, not less. And it most likely will do so in 30years assuming nothing breaks.
There are still 30yrs+ old Nintendo Gameboys that work like a charm and are being refurbished.  
Additionally I can change the behavior of the software as I like. The whole development environment works offline
and can also be installed in an offline virtual machine and will still be able to compile even if the
hardware manufacturer of the main controller board does not exist anymore.  
  
# Videos
Download and watch:  
[Scenario1](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario1.mp4): normal operation  
[Scenario2](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario2.mp4): seeking  
[Scenario3](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario3.mp4): sorting  
[Scenario4](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario4.mp4): auto bookmarks  
[Scenario5](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario5.mp4): sleep timer  
[Scenario6](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario6.mp4): wakeup timer  
[Scenario7](https://github.com/lanmarc77/esp-audio-book-player/raw/main/videos/scenario7.mp4): firmware upgrade  
  
# Hardware
The build only needs a few components that are manufactured by companies in mass and therefore cheap.  
For PCB based components the schematics are available in the module datasheets folder.  
  
**Main controller board:**  
The main controller board is a Heltec WiFi Kit 32 V3. It was chosen because it has a very small deep sleep
current of about 10µA. I measured 15µA with nothing connected to it. Still very good. It already comes with
the circuitry to connect and charge a rechargeable battery as well as the possibility to disconnect other
components from power by using a software programmable power pin (Ve).  
Finally an OLED display is also already available for the UI.  
Be aware that you buy V3 of this board version as it has the low deep sleep current. Otherwise your battery
looses way too much energy even if you do not use the player. Additionally verify that you also get a fitting
battery cable.  
![image](pictures/heltec_wifi_kit32_v3.jpg)  
Typical price in Germany incl. tax (2023): 20€. 
  
**I²S digital analog audio converter**  
Currently only a MAX98357A based converter has been tested extensively. Also an UDA1334A and PCM5102 were
tested and they worked. Both of these stereo DACs needed around 10-15mA more current which will reduce the system
runtime.
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
  
**Rotary encoder**  
To minimize wiring and drilling work I decided for a single rotary encoder as main navigation option. This model
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
  
Pictures of manually wired PCB:  
![image](pictures/pcb_top.jpg)  
![image](pictures/pcb_bottom.jpg)  
  
These PCBs have headers for the components. You can leave these out but then need to make sure in 
which order you solder the wires as you might not get to them anymore.   
  
## Buyable manufactured PCB
WIP: One or two PCB layouts as motherboard for a 3D printed base case and a worldwide buyable case.  
  
## Printed/buyable case
WIP: 3D case model  
TODO: worldwide buyable model  
  
## Current measurements / battery life
The runtime of the system is highly dependent on the current consumption. The current consumption depends on the
type of audio played and mostly the volume. The following measurements were made using the
setup prototype development board as seen above with 8 Ohm speaker OWS-131845W50A-8, 3.7V, 4GB class 10 Transcend
microSD. An UNI-T UT61E with a 22µF tantal smoothing capacitor to handle the current peaks was used. Play speed
setting x1.0.  

  
| scenario                             | volume setting  | current |
|--------------------------------------|-----------:|------------:|
| switched off                         |  0%  | 19µA   |
| switched on, not playing, display on |  0%  | 37ma   |
| switched on, not playing, display off|  0%  | 34ma   |
| 128kbit/s mp3 44100 khz              | 50%  | 42ma   |
| 64kbit/s mp3 44100 khz               | 50%  | 42ma   |
| 128kbit/s mp3 22050 khz              | 50%  | 39ma   |
| 64kbit/s mp3 22050 khz               | 50%  | 39ma   |
| 128kbit/s ogg 44100 khz              | 50%  | 44ma   |
| 64kbit/s ogg 44100 khz               | 50%  | 44ma   |
| 128kbit/s ogg 22050 khz              | 50%  | 41ma   |
| 64kbit/s ogg 22050 khz               | 50%  | 41ma   |
| 24kbit/s amr 16000 khz               | 50%  | 41ma   |
| 128kbit/s mp3 44100 khz              | 75%  | 51ma   |
| 64kbit/s mp3 44100 khz               | 75%  | 51ma   |
| 128kbit/s mp3 22050 khz              | 75%  | 50ma   |
| 64kbit/s mp3 22050 khz               | 75%  | 50ma   |
| 128kbit/s ogg 44100 khz              | 75%  | 52ma   |
| 64kbit/s ogg 44100 khz               | 75%  | 52ma   |
| 128kbit/s ogg 22050 khz              | 75%  | 52ma   |
| 64kbit/s ogg 22050 khz               | 75%  | 52ma   |
| 24kbit/s amr 16000 khz               | 75%  | 52ma   |
| any file from before                 | 100% | ~105mA |
  
At high volumes the energy needed by the speaker dominates the current consumption. Also the current is
not very stable and highly depends on the content that is actually played.  
The volume setting actually follows an x⁴ curve for translating % into power to fit our hearing.  
Depending on play speed settings the current consumption rises. This is due to the fact that more data
needs to be handled and the CPU frequency is stepped up. Approximately 2mA per 0.1 step speed up.  
  
An 18650 battery with 2500mAh will most certainly hold 50hrs assuming average volume levels between 50%...75%.  
  
# UI and player navigation
The following sub chapters explain the states of the player and the different menus or screens the player shows.  
You might want to watch the videos above for a shorter introduction.  
  
## The rotary encoder
The only input device is a rotary encoder with a button. It currently supports the following actions:  

- rotation for number, time and volume selections, speed of rotation in most menus increases the step size of the setup unit
- short click usually used for ok
- long click usually used as cancel or back
- double click activates sub or special functions

## The players state machine
Legend:  
SC=short click  
LC=long click  
DC=double click  
AC=any type of click  
  
```
            powered off
                |
                | AC
                |                                              pressed long
                v       button still pressed?                    enough?   
            init screen ---------------------> reset countdown ---------- 
                |                                 screen                 |
                |                                    | button            |
                |                                    | released     reset screen
                |                                    |          +delete all bookmarks
                |                                    |            and user settings
                |                                    |                   |
                |                                    v                   |
                |<-------------------------------------------------------
                |
                | no sd card found?                      3s
                |--------------------> no sd card screen ---> power off
                |
                | new fw upgrade found?           SC                      10s
                |------------------> upgrade init ----> upgrade countdown --
                |                      screen              screen           |
                |                        |                  |               |
                |                        | any other        | AC         upgrade
                |                        | click            |            progress
                |                        | or no click      |            screen
                |                        v after 3s         v               |
                |<------------------------------------------                | ~10s
                |                                                           |
                |                                                           v
                |                                                         reboot
                |
                | ~2s
                v           0 folders?                               3s
          audio book scan -------------> no audio books found screen ---> power off
              screen
                | ~?s
                | scan time depends on number of audio books
                |
                v                    LC                                  3s
   ---> audio book selection screen ----> persist new fw ---> off screen ---> power off
  |     |     |       ^    ^      ^         if needed
  |     | DC  |SC     |    |      |
  |     |     |       |    |      |
  |     |     |       |    |      |
  |     |     |       |    |      |
  |     |     |       |     ----   -
  |     |     |       | AC      |   |
  |     |     v       |         |   |<-------------------------
  |     |    audio book content |   |                          |
  |     |    |   scan screen    |   |                          |
  |     |    |    |             |   |       next sub screen    |
  |     |    |    | 0 files     |   |       ---------------    |
  |     |    |    | found?      |   |      |               |   |
  |     |    |    |             |   |      v            SC |   |
  |     |    |     -------------    |    play overlay ----     |
  |     |    |                      |    screen                |
  |     |    | scan      -----------     |2s  ^                |
  |     |    | finished |                |    |                |
  |     |    |          | LC             |    |rotate          | LC
  |     |    v          |    SC          v    |   SC           |
  |     |    pause screen  -----------> playing ------------> pause screen
  |     |    with chapter  <----------  screen <------         with time
  |     |     selection            LC     |           |        selection
  |     |        |                        | DC        |            |
  |     |        | DC                     |        play next       | DC
  |     |        |                         -------> chapter        |
  |     |        |                                                 |
  |     |         ------------------------------------------------>|
  |     |                                                          |
  |     v                                                          v
  |  sleep timer                                              wakeup timer
  |  setup screen                                             setup screen
  |  |           |                                            |           |
  |  | SC        | any other click                            | SC        | any other click
  |  | activate  | timer not                                  | activate  | timer not
  |  | timer     | activated                                  | timer     | activated
  |<-            |                                            |           |
  |              |                                             ----- -----
   --------------                                                   |
                                                                    v
                                                           back to corresponding
                                                               pause screen

```

After 30s of no click the screen turns off. If the player is not playing and the screen was off for an additional
30s then the player powers off.  
If the screen is off any click will first turn the screen on. An additional click is needed for the
desired action. There is one exception: the double click in the play screen to skip to the next
chapter works right away even if the screen is off.  
If the battery level drops below 3.35V the low battery screen is shown and the player powers off.  
  
## The menus
All menus are built upon the basic ASCII set. This should allow changing the display easily.  
If something can be setup using rotation usually that element flashes.  
  
### Init screen
```
 ----------------
|                |
|     (o_o)      |
|       ok       |
|   v00.00.05    |
 ----------------
```
Shows the firmware version number. If ok is displayed the firmware is persisted. If TEST is displayed
the firmware is not persisted and if the player powers off automatically the old firmware is reinstalled.  
  
### Reset countdown screen
```
 ----------------
|                |
|     (o o)      |
|       O        |
|[=========-    ]|
 ----------------
```
If the button is hold down until the progress at the bottom has counted down the player
will delete all bookmarks and reset all user settings to default values.  
  
### Reset screen
```
 ----------------
|0000000000000000|
|0000000000000000|
|0000000000000000|
|0000000000000000|
 ----------------
```
Shown until the reset procedure is finished.  
  
### Upgrade init screen
```
 ----------------
| ||    00.00.04 |
|-  -     vvv    |
|-  -      ?     |
| ||    00.00.05 |
 ----------------
```
Shows the currently installed firmware and the new version that can be installed.  
  
### Upgrade countdown screen
```
 ----------------
| ||    00.00.04 |
|-  -     vvv    |
|-  -     5s     |
| ||    00.00.05 |
 ----------------
```
A countdown counts down to signal the upgrade will be started.  
  
### Upgrade screen
```
 ----------------
|       ||       |
|    -  34% -    |
|    -      -    |
|       ||       |
 ----------------
```
Progress is shown during the running upgrade procedure.  
  
### No SD card screen
```
 ----------------
|    -------     |
|   | SD ?  |    |
|   |______/     |
|                |
 ----------------
```
This screen is also shown if the SD card is removed while the player is active.  
  
### Audio book scan screen
```
 ----------------
|     |---|      |
|     | ? |      |
|     |---|      |
|[===    .      ]|
 ----------------
```
The progress bar at the bottom shows the progress of the scan an sorting process.  
  
### No audio books found screen
```
 ----------------
|     |---|      |
|     | 0 |      |
|     |---|      |
|                |
 ----------------
```
### Audio book selection screen
```
 ----------------
|audio book name |
| |------------| |
| |     12/121 | |
| |----00:01---| |
 ----------------
```
Using rotation the different audio books can be selected. The screen shows selected 12 of 121 audio books.
The audio book name at the top is essentially the name of the folder and scrolls through the screen if it is long.  
The value 00:01 is only shown if the wake up timer has been setup and activated and shows the setup value.  
  
### Audio book content scan screen
```
 ----------------
| |------------| |
| |  A <==> B  | |
| |------------| |
|[=====- .      ]|
 ----------------
```
The progress bar at the bottom shows the progress of the scan and sorting process and A and B are animated.  
  
### Pause screen
```
 ----------------
|audio book name |
|      3/12 3m  8|
|     12:22 |>  9|
|[===-   .      ]|
 ----------------
```
The audio book name at the top is essentially the name of the folder and scrolls through the screen if it is long.  
3/12 shows the currently selected chapter (=file) of this audio book and the number of all chapters.  
12:22 is the current time within the currently selected chapter.  
Depending on the mode either the time or the chapter flashes meaning this can be changed (seek or skip) using rotation.  
The value 3m is only displayed if a sleep timer was setup and activated and shows the time left until the player will power off.  
The progress bar at the bottom shows the progress within the current chapter.  
On right side the battery level in percent is shown.  
    
### Playing screen
```
 ----------------
|audio book name |
|      3/12 3m  8|
|     12:22 ||  9|
|[===-   .      ]|
 ----------------
```
The audio book name at the top is essentially the name of the folder and scrolls through the screen if it is long.  
3/12 shows the currently selected chapter (=file) of this audio book and the number of all chapters.  
12:22 is the current time within the currently selected chapter.  
The time is switching repeatedly between current time position and time left.  
The value 3m is only displayed if a sleep timer was setup and activated and shows the time left until the player will power off.  
The progress bar at the bottom shows the progress within the current chapter.  
On right side the battery level in percent is shown.  
  
### Play overlay screen
```
     volume               play speed
 ----------------      ----------------  
|################|    |                |
|################| -> |     >>>>>>>>   |
|##              |    |      x1.20     |
|      5100      |    |                |
 ----------------      ----------------
        ^                     |
        |                     v
  repeat mode              equalizer
 ----------------      ----------------  
|                |    |                |
|  |>  -->  ||   | <- |     normal     |
|                |    |                |
|                |    |                |
 ----------------      ----------------
```
The play overlay screen is activated when a file is played and rotation is used. If it is active a short click
is going through the different settings. This screen will automatically switch back to the play screen after 2s of
no user input but will remember for 8 more seconds at which sub menu it was left.  
  
Volume: The # characters are a visual representation of the setup volume. Volume level ranges from 0...10000. 7500 actually
marks 100% audio volume of the source everything above this will add a gain of up to 25% to the source volume level.  
  
Play speed: Using rotation the speed can be setup. Play speed is changed instantly and stored individually for each audio book.  
  
Equalizer:  Using rotate you can choose between the different equalizer presets. The setting is stored individually
for each audio book.  
  
Repeat mode:  Using rotate one can choose to enable folder repeat (<-->) or not (-->). The setting is stored individually
for each audio book.  
  

### Sleep timer setup screen
```
 ----------------
|                |
|    (-_-)Zzz    |
|      1/99      |
|                |
 ----------------
```
Using rotation the sleep timer can be setup from 1 minute to 99 minutes in minute precision.  
  
### Wake up timer setup screen
```
 ----------------
|                |
|(-_-)  ->  (o_o)|
|                |
|      01:00     |
 ----------------
```
Using rotation the sleep timer can be setup from 1 minute to to 24 hours in minute precision.  
  
### Low battery screen
```
 ----------------
|      _--_      |
|     |    |     |
|     |    |    6|
|     |####|     |
 ----------------
```
On right side the battery level in percent is shown.  
  
### Off screen
```
 ----------------
|                |
|    (-_-)Zzz    |
|    241160MB    |
|       2%       |
 ----------------
```
Also shows the size of the detected SD card and the used space of the bookmark storage in the SPIFFS
of the ESP32.  
  
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
If you wish to upgrade shortly press the encoder button. A countdown of 10s gives you the possibility
to stop the upgrade if you press the encoder button. Once the upgrade started you can not interrupt it.
The actual writing process for the upgrade takes around 10s and the progress is shown.  
  
If everything worked you should now see the device restart with the new firmware version but it displays
"TEST" instead of "ok" in the initial firmware version screen during startup.  
"TEST" means that the new firmware is not yet persisted. In this test state you have the possibility
to test everything and check if the new version suits your needs.  
You can persist the new firmware by manually switching the device off by long pressing the encoder button
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
  
# Software architecture
  
```

 -----------------
|   ssd1306*.*    |           -----------------------
|                 |          |       battery.*       |
| low level I²C   |          |                       |
| functions for   |          | monitors, filters     |
| OLED and fonts  |          | and offers the        |
|                 |          | current battery level |
 -----------------           |                       |
         ^                   | uses task             |
         |                   |                       |
         | calls              -----------------------
         |                     ^
 --------------------          |
|   ui_elements.*    |         | calls
|                    |         |
| offers reusable    |         |
| UI elements        |         |                                   --------------------------
|                    |         |    ---------------------------   |         sd_play.*        |
 --------------------          |   |     rotary_encoder.*      |  |                          |
           ^                   |   |                           |  | plays files from SD card |
           |                   |   | low level rotary encoder  |  | using esp-adf pipelines  |
           | calls             |   | and switch handling       |  |                          |
           |                   |   |                           |  |                          |
 ------------------------      |   | uses task + interrupt     |  | uses tasks               |
|       screens.*        |     |   |                           |  |                          |
|                        |     |    ---------------------------    --------------------------
| creates and displays   |     |     ^                              ^                      |
| complete screen        |     |     |                              |                      |
| layouts                |     |     | calls                        | calls                |
|                        |<-   |     | queue communication          | queue communication  |
 ------------------------   |  |     |                              |                      |
                            |  |     |           -------------------                   ----
                      calls |  |     |          |                                     |
                            |  |     v          v                               calls |
 -----------             --------------------------                                   v
|   main.*  |           |        ui_main.*         |             --------------------------
|           |  calls    |                          |            |      format_helper.*     |
|  basic    | --------> |    main player logic     |    calls   |                          |
|  board    |           |                          | ---------> | functions to get audio   |
|  init and |           |   state machine based    |            | format specific          |
|  deinit   |           |                          |            | information like         |
|           |           |                          |            | runtime and bitrate      |
 -----------            |                          |            |                          |
                        |                          |             --------------------------
       ---------------- |                          |
      |                 |                          |<-----------
      |                  --------------------------             |
      |                                |                        |
      | calls                          | calls                  | calls +
      |                                |                        | queue communication
      v                                v                        v
 --------------------         ----------------------         -----------------------------
|     sd_card.*      |       |        saves.*       |       |        ff_handling.*        |
|                    |       |                      |       |                             |
|  low level SPI     |       | handles save         |       | reads and sorts dirs/files  |
|  bus handling and  |       | restore and house    |       | an returns sorted lists     |
|  VFS mount/umount  |       | keeping of bookmarks |       |                             |
|                    |       | and player settings  |       | uses task for sorting       |
 --------------------        |                      |       |                             |
                              ----------------------         ------------------------------

```  
The following sub chapters will give an insight into each shown component and its architecture ideas.  
This layout shows the ideal model that only very rarely is broken in the code.  
  
## main.*
The entry point of the application. It initializes the board and sets up the CPU, disables WiFi and
hands over to the other components.  
Function main() is never left but if other components finish powers off the board and sets up a
wake up timer if needed.  
  
## ui_main.*
This component implements the main player logic. A state machine represents the navigation of the user
and the state the player is currently in.  
As the central component it makes direct use of a all the other components.  
  
## sd_card.*
This component initializes the SPI bus for usage with an SD card and tries to mount the SD card
into the ESPs VFS so it can be used by all other components using file access functions.  
  
## rotary_encoder.*
This component initializes and monitors the rotary encoder and switch using a task and and interrupt.  
Events are generated and placed as messages into a queue for communicating with other components.  
  
## battery.*
This component initializes and monitors the AD converter for monitoring the battery level.  
There are two battery levels available. With slower noise filtering and with faster more noisy filtering.  
A task samples the battery voltage every 500ms.  
  
## screens.\*, ui_elements.\*, ssd1306\*.\*
This collection of components uses a typical layered approach.  
The ssd1306 functions give access to a physical display. Here the OLED of the board.  
The ui_elements functions use the ssd306 functions to offer reusable UI elements like text output or
progress meters with which larger screen layouts can be built.  
The screens functions use the ui_elements functions to create a complete screen layout for the player.  
It is assumed that the display can handle a layout of 16x4 ASCII characters.  
  
## saves.*
This component handles the storage, retrieval and house keeping of bookmarks and the settings (like volume)
if the player. It makes use of the ESP VFS system and currently stores everything in the internal SPIFFS.  
It also takes care of initializing the SPIFFS.  
  
## ff_handling.*
This component scans folders and files within folders and delivers a sorted list. It uses a task for the
longer running scanning and sorting process and communicates its progress via a queue. The scanning and
sorting process can also be canceled via queue communication. During scanning and sorting the CPU
frequency is increased to speed up the process.  
  
## format_helper.*
This component offers audio file format functions. One can convert byte position <-> time stamp and
determine sample rates and bitrates and further information to setup the esp-adf pipeline correctly
right from the start.  
  
## sd_play.*
This components tries to abstract all the esp-adf functionality of playing an audio stream with in the pipeline.
It combines SD card access by allowing to start playing a file from the SD card using a task.  
Using queues the components can be instructed to start/stop the playing and contentiously reports the current
playing position for the user interface.  
The resume function that actually is not out of the box supported by a pipeline based streaming approach is
implemented by adding an own file stream handler within the pipeline that takes care of at first delivering needed
header information for the decoder but then jumps within the file to the position to resume from.  
  
## naming conventions
Filenames and global variables/functions match each others prefixes to easily be able to locate them.  
Prefixes are written in capital case and use _ as separator.  
Apart from the prefixes variables and functions use camel case.  
  
# Limitations, design decisions and behavior descriptions
The following sub chapters explain a few design decisions, limitations and system behavior.  
  
## Sorting, limits of files/folders
Sorting on a µC is hard. Sorting algorithms trade speed with memory and memory is something a µC usually does not have
very much. It is impossible to keep a huge root directory in memory depending on the the character length of each
entry. Especially if you want to be deterministic on how many entries you support.  
This projects implementation has two options of displaying and playing sorted entries.  
  
### Sorting each time
This is the default implementation. After startup when the list of all available audio books is read this list is sorted.
Instead of keeping the strings/names of all entries in memory only their file allocation table position is used. This position
acts as kind of ID of the entry. This ID is then sorted inside an array but not by its value but by its corresponding name.
But this also means that the algorithm always has to translate between ID and name of the file resulting in an increased
time of sorting. The algorithm used is a selection sort with a complexity of O(n²). But it is memory deterministic.  
This allows keeping 1000 entries with only 2kB of memory usage. To wait for 1000 entries to be sorted is also
still acceptable. Sorting speed is also determined by the way entries are named. As the C function strcmp is starting to compare
at the beginning of a string the earlier a mismatch happens the earlier it can stop. It is beneficial to name files e.g.
01... , 02... instead of ...01, ...02.  
If a directory contains more than 1000 entries they are not taken into account.  
This described sorting applies for audio books (directories) and their content (files).  
  
### Presorted list of root directory
As I wanted the player to be able to also hold large collections of audio books 1000 entries in the root directory
did not seem enough.  
But sorting would take too long to be acceptable. Instead a presorted entry list can be created. Much like an .m3u file.  
The file needs be called `presorted.txt` and must reside in the root directory of the SD card. It can contain 9999 lines
where each line contains the directory name. The order in the file is the order in which they are displayed in
the player.  
The following command chain can be used in the root directory of the SD card to create such a sorted file:  
  
`find * -type d -print | sort | iconv -t 437 > presorted.txt`  
  
Names of entries must be in code page 437 format as this is the only one that is enabled in the ESP32. I did not wanted
to make things more complicated by supporting all kinds of character encodings.  
9999 entries=audio books with an average length of an audio book of 6hrs would take almost 55 years of listening if
you listen every single day for 3hrs.  
  
Assuming AMR-WB file format with 24kbit/s the SD card would need to have ~618GB.  
Assuming MP3 file format with 128kbit/s the SD card would need to have ~3.3TB.  
  
As the files within a directory/audio book are usually below 1000 the presorted.txt is not implemented for these.
  
## Bookmarks
I decided to implement the bookmark storage inside the SPIFFS of the ESP32. I reserved 512kB for the SPIFFS. The
storage could also have been the SD card and would have been the first choice. But that would mean writing to the
SD card. The same card where the audio books are stored. I did not wanted to risk file system problems if one of those
writes failed. Meaning a destroyed file system not being able to listen to audio books anymore. This project never
writes to the SD card which in turn never can get corrupted.  
Each bookmark is written when the player is paused or is powered off by one of the automatic methods like low battery
or sleep timer. A bookmark is a file in the SPIFFS. The name of the file is an MD5 hash constructed from the directory
name of the audio book. Inside the bookmark the listening position is stored and the file name that was currently played.  
  
Depending on the length of the file name bookmarks take more or less space in SPIFFS. On average 1kb can be assumed. This
means around 500 bookmarks can be stored. Deleting individual bookmarks is not possible. But by using the reset procedure
all bookmarks can be deleted. The off screen displays the usage of the SPIFFS in percent.  
There is an automatic house keeping implemented. If the SPIFFS usage level is above 75% the SD card and the bookmarks are
scanned. If there are bookmarks for which no fitting directory on the SD card exists this bookmark is deleted. This house
keeping is based on the use case that SD cards are being swapped and around 500 audio books is the average on one of the
SD cards.  
  
Manual bookmarks are not supported. Mainly because I personally never needed them but also because house keeping gets
much more complicated as well as a method of jumping to different bookmarks inside the same audio book must be implemented.  
  
## CBR/VBR
Only fully supported are CBR encoded files. The player also plays VBR files but the displayed timestamps might be wrong.
Without scanning the full file or implementing code for the various VBR indexes there is no way to determine a correlation
of file position and time.  
The format handler functions take a look at the first frame of a file to determine the bitrate and therefore the frame length
of a file.  
  
## Metadata
No metadata of files is used. This includes chapter indexes, subtitles or cover pictures.  
  
## Bluetooth audio
The used µC board uses the ESP32-S3. This newer model of the ESP32 does not support the so called classic Bluetooth
audio stack which uses SBC as codec. It only supports the not yet widely used Bluetooth Audio LE. The esp-adf does
not yet support a pipeline element that uses Bluetooth Audio LE. Once support is added and devices are getting more common
I might implement support for it.  
It might also be possible to make this project compile with the classic ESP32 that does support classic Bluetooth using SBC.
But I do not plan for this as I use headphones with a jack and no extra battery I need to take care of.  
  
## Audio processing
The ESP32 starts with the lowest possible CPU speed of 80Mhz. This is not enough CPU power if certain audio processing
options are enabled. Especially if the play speed has a high setting 80Mhz is not enough for the calculations needed. 
The program increases CPU speed to 160Mhz when play speed is getting greater than 1 and to 240Mhz for value greater 2.
This will increase power consumption and reduce system runtime.  
  
# Ideas/future
- multi user support with individual bookmarks
- audio navigation for handicapped people
- printed PCB with fitting 3D printable case or fitting for a cheap buyable case

