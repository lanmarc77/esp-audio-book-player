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
#ifndef UI_ELEMENTS_C
#define UI_ELEMENTS_C

#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "ui_main.h"
#include "ui_elements.h"
#include <string.h>
#include <esp_log.h>

bool UI_ELEMENTS_displayDark=false;
#define I2C_MASTER_SCL_IO 18        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 17        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1    /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */


ssd1306_handle_t SSD1306_dev = NULL;
void SSD1306_deInit(){
    ssd1306_clear_screen(SSD1306_dev, 0x00);
    esp_rom_gpio_pad_select_gpio(21);
    gpio_set_direction(21, GPIO_MODE_OUTPUT);
    gpio_set_level(21,0);//pin21 OLED RST
}

void SSD1306_init(){
    esp_rom_gpio_pad_select_gpio(21);
    gpio_set_direction(21, GPIO_MODE_OUTPUT);
    gpio_set_level(21,1);//pin21 OLED RST

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    
    SSD1306_dev = NULL;
    SSD1306_dev = ssd1306_create(I2C_MASTER_NUM, SSD1306_I2C_ADDRESS);
    ssd1306_refresh_gram(SSD1306_dev);
    ssd1306_clear_screen(SSD1306_dev, 0x00);
}

void SSD1306_cls(){
    ssd1306_clear_screen(SSD1306_dev, 0x00);
    ssd1306_refresh_gram(SSD1306_dev);
}

void SSD1306_printStr(uint8_t x,uint8_t y, char* string){
    uint16_t i=0;
    if(UI_ELEMENTS_displayDark==false) for(i=0;string[i]!=0;i++) ssd1306_draw_char(SSD1306_dev, (x+i)*8,y*16, string[i],16,1);
}

/**
  * @brief shows the progress meter
  *
  * @param percent percent 0...100
  * 
  */
 void UI_ELEMENTS_progressBar(uint8_t percent){
    char progress[17];
    uint8_t i;
    uint8_t numCharsBig=(14*percent)/100;
    uint8_t numCharsRemainder=(14*percent)%100;

    memset(&progress[0],' ',sizeof(progress));
    progress[0]='[';progress[15]=']';if(percent<50) progress[8]='.';progress[16]=0;
    for(i=0;i<numCharsBig;i++){
        progress[i+1]='=';
    }
    if(numCharsRemainder>=66){
        progress[i+1]='-';
    }else if(numCharsRemainder>=33){
        progress[i+1]='_';
    }
    SSD1306_printStr(0,3,&progress[0]);
}

int32_t UI_ELEMENTS_lastSecondsDisplayed=-1;
/**
  * @brief shows a time in hours minutes seconds
  *
  * @param playedS time to display in seconds
  * @param displayMode 0=normal,1=hours+minutes blink,2=playedS is displayed as negative number
  * 
  */
void UI_ELEMENTS_timePlayed(int32_t playedS,uint8_t displayMode){
    char timeString[17];
    if((UI_ELEMENTS_lastSecondsDisplayed>=0)||(UI_ELEMENTS_lastSecondsDisplayed!=playedS)){
        UI_ELEMENTS_lastSecondsDisplayed=playedS;
        uint16_t hours=playedS/3600;
        uint8_t minutes=(playedS%3600)/60;
        uint8_t seconds=(playedS%3600)%60;
        uint64_t now=esp_timer_get_time();
        if((displayMode==1)&&(((now/1000)%1000)<100)){
            if(hours==0){
                sprintf(&timeString[0],"       :%02d",seconds);
            }else{
                sprintf(&timeString[0],"    :  :%02d",seconds);
            }
        }else{
            if(displayMode==2){
                if(hours==0){
                    sprintf(&timeString[0],"    -%02d:%02d",minutes,seconds);
                }else{
                    sprintf(&timeString[0]," %3d:%02d:%02d",-hours,minutes,seconds);
                }
            }else{
                if(hours==0){
                    sprintf(&timeString[0],"     %02d:%02d",minutes,seconds);
                }else{
                    sprintf(&timeString[0],"%4d:%02d:%02d",hours,minutes,seconds);
                }
            }
        }
        SSD1306_printStr(0,2,&timeString[0]);
    }
}

uint16_t UI_ELEMENTS_battCurveV[11]={4200,4100,4000,3900,3800,3700,3600,3500,3400,3300,3200};
uint8_t UI_ELEMENTS_battCurveP[11] ={100,94,84,74,62,52,39,22,13,3,0};

uint8_t UI_ELEMENTS_getPercentFromBattVotage(uint16_t battVoltage){
    uint8_t p=0;
    if(battVoltage>=UI_ELEMENTS_battCurveV[0]){
        p=100;
    }else if(battVoltage<=UI_ELEMENTS_battCurveV[10]){
        p=0;
    }else{
        uint16_t upper=0,lower=0;
        for(uint8_t i=0;i<11;i++){
            if(battVoltage>=UI_ELEMENTS_battCurveV[i]){
                upper=UI_ELEMENTS_battCurveV[i-1];
                lower=UI_ELEMENTS_battCurveV[i];
                p=(100*(upper-battVoltage))/(upper-lower);
                //ESP_LOGI("UI_ELEMENTS","P1: %d u:%d l:%d",p,upper,lower);
                upper=UI_ELEMENTS_battCurveP[i-1];
                lower=UI_ELEMENTS_battCurveP[i];
                p=upper-(p*(upper-lower))/100;
                //ESP_LOGI("UI_ELEMENTS","P2: %d u:%d l:%d",p,upper,lower);
                return p;
            }
        }
    }
    return p;
}

/**
  * @brief displays a battery level
  *
  * @param value battery level in mV
  * 
  */
void UI_ELEMENTS_batteryIndicator(uint32_t value){
    char b[12];
    uint8_t percent=0;
    uint64_t now=esp_timer_get_time();
    if(((now/1000)%10000)>10000){
        sprintf(b,"%1lu",value/1000);
        SSD1306_printStr(15,0,b);
        sprintf(b,"%1lu",(value%1000)/100);
        SSD1306_printStr(15,1,b);
        sprintf(b,"%1lu",((value%1000)%100)/10);
        SSD1306_printStr(15,2,b);
    }else{
        percent=UI_ELEMENTS_getPercentFromBattVotage(value);
        //ESP_LOGI("UI_ELEMENTS","V: %lu P: %d",value,percent);
        if(percent>=100){
            sprintf(b,"%1d",percent/100);
            SSD1306_printStr(15,0,b);
        }
        if(percent>=10){
            sprintf(b,"%1d",(percent%100)/10);
            SSD1306_printStr(15,1,b);
        }
        sprintf(b,"%1d",(percent%100)%10);
        SSD1306_printStr(15,2,b);
    }
}

/**
  * @brief displays different type of huge symbols
  *
  * @param symbol symbol number to display
  * 
  */
void UI_ELEMENTS_mainSymbol(uint8_t symbol){
    uint64_t now=esp_timer_get_time();
    char b[17];
    b[0]=0;
    if(symbol==1){//play symbol
        SSD1306_printStr(11,2,"|> ");
    }else if(symbol==2){//pause symbol
        SSD1306_printStr(11,2,"|| ");
    }else if(symbol==3){//no SD card symbol
        SSD1306_printStr(0,0,"    -------     ");
        SSD1306_printStr(0,1,"   | SD ?  |    ");
        SSD1306_printStr(0,2,"   |______/     ");
    }else if(symbol==4){//all folder scan symbol
        SSD1306_printStr(0,0,"     |---|      ");
        SSD1306_printStr(0,1,"     | ? |      ");
        SSD1306_printStr(0,2,"     |---|      ");
    }else if(symbol==5){//no folder found symbol
        SSD1306_printStr(0,0,"     |---|      ");
        SSD1306_printStr(0,1,"     | 0 |      ");
        SSD1306_printStr(0,2,"     |---|      ");
    }else if(symbol==6){//folder selection
        SSD1306_printStr(0,1," |------------| ");
        SSD1306_printStr(0,2," |            | ");
        SSD1306_printStr(0,3," |------------| ");
    }else if(symbol==7){//single folder scan symbol
        if(((now/1000)%1000)<500){
            SSD1306_printStr(0,0," |------------| ");
            SSD1306_printStr(0,1," |  B <==> A  | ");
            SSD1306_printStr(0,2," |------------| ");
        }else{
            SSD1306_printStr(0,0," |------------| ");
            SSD1306_printStr(0,1," |  A <==> B  | ");
            SSD1306_printStr(0,2," |------------| ");
        }
    }else if(symbol==8){//switch off
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"    (-_-)Zzz    ");
        SSD1306_printStr(0,2,"                ");
        SSD1306_printStr(0,3,"                ");
    }else if(symbol==9){//starting
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"     (o_o)      ");
        SSD1306_printStr(0,2,"                ");
        sprintf(&b[0],"   v%02X.%02X.%02X    ",FW_MAJOR,FW_MINOR,FW_PATCH);
        SSD1306_printStr(0,3,&b[0]);
    }else if(symbol==10){//erasing countdown
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"     (o o)      ");
        SSD1306_printStr(0,2,"       O        ");
    }else if(symbol==11){//erasing
        SSD1306_printStr(0,0,"0000000000000000");
        SSD1306_printStr(0,1,"0000000000000000");
        SSD1306_printStr(0,2,"0000000000000000");
        SSD1306_printStr(0,3,"0000000000000000");
    }else if(symbol==12){//battery low
        SSD1306_printStr(0,0,"      _--_      ");
        SSD1306_printStr(0,1,"     |    |     ");
        SSD1306_printStr(0,2,"     |    |     ");
        SSD1306_printStr(0,3,"     |####|     ");
    }else if(symbol==13){//pause symbol with repeat folder option on
        SSD1306_printStr(11,2,"||*");
    }else if(symbol==14){//fw upgrade init
        sprintf(&b[0]," ||    %02X.%02X.%02X ",FW_MAJOR,FW_MINOR,FW_PATCH);
        SSD1306_printStr(0,0,&b[0]);
        SSD1306_printStr(0,1,"-  -     vvv    ");
        SSD1306_printStr(0,2,"-  -      ?     ");
        SSD1306_printStr(0,3," ||    XX.XX.XX ");
    }else if(symbol==15){//fw upgrade running
        SSD1306_printStr(0,0,"       ||       ");
        SSD1306_printStr(0,1,"    -      -    ");
        SSD1306_printStr(0,2,"    -      -    ");
        SSD1306_printStr(0,3,"       ||       ");
    }else if(symbol==16){//wakeup timer
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"(-_-)  ->  (o_o)");
        SSD1306_printStr(0,2,"                ");
        SSD1306_printStr(0,3,"                ");
    }else if(symbol==17){//screen rotation
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"   -------->    ");
        SSD1306_printStr(0,2,"  |   ???   |   ");
        SSD1306_printStr(0,3,"   <--------    ");
    }else if(symbol==18){//rotary encoder direction
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1," o o            ");
        SSD1306_printStr(0,2,"o   o           ");
        SSD1306_printStr(0,3," o o            ");
    }else if(symbol==19){//rotary encoder speed
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1," o o            ");
        SSD1306_printStr(0,2,"o   o           ");
        SSD1306_printStr(0,3," o o            ");
    }else if(symbol==20){//bookmark deletion
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"   Bookmarks    ");
        SSD1306_printStr(0,2,"?????? -> 0     ");
        SSD1306_printStr(0,3,"       ?        ");
    }else if(symbol==21){//brightness setup
        SSD1306_printStr(0,0,"                ");
        SSD1306_printStr(0,1,"\\\\\\\\\\\\   //////");
        SSD1306_printStr(0,2,"      xxx       ");
        SSD1306_printStr(0,3,"//////   \\\\\\\\\\\\");
    }
}

/**
  * @brief allows to display a number selection element
  * 
  * @param x position on screen
  * @param y position on screen
  * @param selectedNumber the currently selected number
  * @param amountOfNumbers highest number to select
  * @param blinkMode 0=number is displayed solid, 1=number is blinking
  *
  */
void UI_ELEMENTS_numberSelect(uint8_t x,uint8_t y,uint16_t selectedNumber,uint16_t amountOfNumbers,uint8_t blinkMode){
    char chapterString[17];
    char chapterCompleteString[17];
    uint64_t now=esp_timer_get_time();
    if(blinkMode==1){
        if(((now/1000)%1000)<100){
            sprintf(&chapterString[0],"/%d",amountOfNumbers);
        }else{
            sprintf(&chapterString[0],"%d/%d",selectedNumber,amountOfNumbers);
        }
    }else{
        sprintf(&chapterString[0],"%d/%d",selectedNumber,amountOfNumbers);
    }
    sprintf(&chapterCompleteString[0],"%10s",&chapterString[0]);
    SSD1306_printStr(x,y,&chapterCompleteString[0]);
}

/**
  * @brief allows to display a time element in hours and minutes
  * 
  * @param x position on screen
  * @param y position on screen
  * @param secondsTime the currently selected time
  * @param blinkMode 0=time is displayed solid, 1=time is blinking
  *
  */
void UI_ELEMENTS_timeSelect(uint8_t x,uint8_t y,uint64_t secondsTime,uint8_t blinkMode){
    char timeString[17];
    uint64_t now=esp_timer_get_time();
    if(blinkMode==1){
        if(((now/1000)%1000)<100){
            sprintf(&timeString[0],"  :  ");
        }else{
            sprintf(&timeString[0],"%02i:%02i",(uint16_t)(secondsTime/3600),(uint16_t)((secondsTime%3600)/60));
        }
    }else{
        sprintf(&timeString[0],"%02i:%02i",(uint16_t)(secondsTime/3600),(uint16_t)((secondsTime%3600)/60));
    }
    SSD1306_printStr(x,y,&timeString[0]);
}

/**
  * @brief shows equalizer preset
  * 
  * @param equalizer preset, 0=netural
  *
  */
void UI_ELEMENTS_equalizer(uint8_t equalizer){
    switch(equalizer){
        case 0: 
                SSD1306_printStr(5,1,"normal");
                break;
        case 1: 
                SSD1306_printStr(6,1,"bass");
                break;
        case 2: 
                SSD1306_printStr(1,1,"bass and treble");
                break;
        case 3: 
                SSD1306_printStr(5,1,"treble");
                break;
        case 4: 
                SSD1306_printStr(3,1,"headphone");
                break;
        case 5: 
                SSD1306_printStr(5,1,"voice 1");
                break;
        case 6: 
                SSD1306_printStr(5,1,"voice 2");
                break;
    }
}

/**
  * @brief shows repeat mode
  * 
  * @param repeatMode 0=no repeat, bit 1=folder repeat enabled, bit 2=autostart enabled
  *
  */
void UI_ELEMENTS_repeatMode(uint8_t repeatMode){
    if(repeatMode&UI_MAIN_REPEAT_FOLDER){
        SSD1306_printStr(3,1,"|>  <->  ||");
    }else{
        SSD1306_printStr(3,1,"|>  -->  ||");
    }
}

/**
  * @brief shows play speed
  * 
  * @param playSpeed 50...300 meaning x0.5...x3.0
  *
  */
void UI_ELEMENTS_playSpeed(uint16_t playSpeed){
    char number[10];
    SSD1306_printStr(5,1,">>>>>>>>");
    sprintf(&number[0]," x%i.%02i ",playSpeed/100,playSpeed%100);
    SSD1306_printStr(5,2,&number[0]);
}

/**
  * @brief shows a volume meter full screen
  * 
  * @param volume volume level to show 0...10000
  *
  */
void UI_ELEMENTS_volume(int64_t volume){
    char screen[16*4];
    char line[17];
    char number[10];
    uint8_t i=0;
    line[16]=0;
    for(i=0;i<sizeof(screen);i++){
        if(((16*4*volume)/10000)>i){
            if(i>=16*3){
                screen[i]='+';
            }else{
                screen[i]='#';
            }
        }else{
            screen[i]=' ';
        }
    }
    sprintf(&number[0]," %5llu ",volume);
    if(volume>5000){
        memcpy(&screen[20],&number[0],7);
    }else{
        memcpy(&screen[36],&number[0],7);
    }

    for(i=0;i<4;i++){
        memcpy(&line[0],&screen[16*i],16);
        SSD1306_printStr(0,i,&line[0]);
    }
}

uint16_t UI_ELEMENTS_lastTextScrollyPos=0;
uint64_t UI_ELEMENTS_lastTextScrollyTimestamp=0;
/**
  * @brief resets the textScrolly state machine so that the next call
  *        is handled as it would be the first (beginning of text visible)
  * 
  * @param x position on screen
  * @param y position on screen
  * @param length the reserved maximum available length in chars on the display to scroll within
  *
  */
void UI_ELEMENTS_textScrollyReset(uint8_t x,uint8_t y, uint8_t length){
    char scrollyText[17];
    uint8_t i;
    for(i=0;i<length;i++){
        scrollyText[i]=' ';
    }
    scrollyText[i]=0;
    UI_ELEMENTS_lastTextScrollyTimestamp=0;
    UI_ELEMENTS_lastTextScrollyPos=0;
    SSD1306_printStr(x,y,&scrollyText[0]);
}

/**
  * @brief implements a general text scrolly that only scrolls if needed
  *        and can therefore also be used as simple text output
  *
  * @param x position on screen
  * @param y position on screen
  * @param length the reserved maximum available length in chars on the display to scroll within
  * @param string pointer to the text to display
  * 
  */
void UI_ELEMENTS_textScrolly(uint8_t x,uint8_t y, uint8_t length, char *string){
    char scrollyText[17];
    uint8_t sl=strlen(string);
    memset(&scrollyText[0],0,sizeof(scrollyText));
    if(UI_ELEMENTS_lastTextScrollyTimestamp!=0){
        if(UI_ELEMENTS_lastTextScrollyPos==0){//full display length before scrolling starts is longer
            if((esp_timer_get_time()-UI_ELEMENTS_lastTextScrollyTimestamp)/1000>100*length){
                UI_ELEMENTS_lastTextScrollyPos++;
                UI_ELEMENTS_lastTextScrollyTimestamp=esp_timer_get_time();
            }
        }else if((esp_timer_get_time()-UI_ELEMENTS_lastTextScrollyTimestamp)/1000>125){
            UI_ELEMENTS_lastTextScrollyPos++;
            UI_ELEMENTS_lastTextScrollyTimestamp=esp_timer_get_time();
        }
    }else{
        UI_ELEMENTS_lastTextScrollyTimestamp=esp_timer_get_time();
    }
    if(UI_ELEMENTS_lastTextScrollyPos>=sl) UI_ELEMENTS_lastTextScrollyPos=0;
    if(length>=sl){//no need to scroll if the string fits into the assigned space completely
        UI_ELEMENTS_lastTextScrollyPos=0;
    }
    for(uint8_t i=0;i<length;i++){
        if(UI_ELEMENTS_lastTextScrollyPos+i>=sl){
            scrollyText[i]=' ';
        }else{
            scrollyText[i]=string[UI_ELEMENTS_lastTextScrollyPos+i];
        }
    }
    SSD1306_printStr(x,y,&scrollyText[0]);
}

/**
  * @brief displays a countdown time in hours or minutes or seconds as two digit number
  *        in 3 char wide frame
  *
  * @param secondsLeft countdown in seconds
  * 
  */
void UI_ELEMENTS_sleepTimeLeft(uint32_t secondsLeft){
    char timeString[12];
    if(secondsLeft==0){
        return;
    }
    if(secondsLeft>3599){
        sprintf(&timeString[0],"%02luh",(secondsLeft+29)/3600);
    }else if(secondsLeft>99){
        sprintf(&timeString[0],"%02lum",(secondsLeft+29)/60);
    }else{
        sprintf(&timeString[0],"%02lus",secondsLeft);

    }
    SSD1306_printStr(11,1,&timeString[0]);

}

/**
  * @brief updates the screen from the internal buffer
  *        clears the screen if the display should be off
  *
  */
void UI_ELEMENTS_update(){
    if(UI_ELEMENTS_displayDark==false){
        ssd1306_refresh_gram(SSD1306_dev);
    }else{
        SSD1306_cls();
    } 
}

/**
  * @brief clears the screen, works instantly
  *
  */
void UI_ELEMENTS_cls(){
    ssd1306_clear_screen(SSD1306_dev, 0x00);
}

/**
  * @brief switches the display on by internally enabling the update function
  *
  */
void UI_ELEMENTS_displayOn(){
    UI_ELEMENTS_displayDark=false;
}

/**
  * @brief checks if the display is off by internally disabled update function
  *
  * @return true=display is off, false=display is on and being updated
  */
bool UI_ELEMENTS_isDisplayOff(){
    return UI_ELEMENTS_displayDark;
}

/**
  * @brief switches the display off by internally disabling the update function
  *        also clears the screen instantly
  *
  */
void UI_ELEMENTS_displayOff(){
    UI_ELEMENTS_displayDark=true;
    SSD1306_cls();
}

/**
  * @brief initializes the physical display and clears it
  *
  */
void UI_ELEMENTS_init(){
    SSD1306_init();
    SSD1306_cls();
}

/**
  * @brief rotates the screen by 180°
  * 
  * @param rotate 0=normal,1=rotate
  *
  */
 void UI_ELEMENTS_rotate(uint8_t rotate){
    ssd1306_rotate(SSD1306_dev,rotate);
}


/**
  * @brief sets the brightness of the used display
  * 
  * @param rotate 0=off/minimum ...255=maximum
  *
  */
void UI_ELEMENTS_setBrightness(uint8_t brightness){
    ssd1306_setContrast(SSD1306_dev,brightness);
}

#endif
