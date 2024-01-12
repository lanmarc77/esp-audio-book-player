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
#ifndef BATTERY_H
#define BATTERY_H
/*
    This component starts a task to monitor the battery voltage
    Battery voltage is taken every 0.5s and send through a fast but
    jumpy digital filter and through a slow but stable filter
    battery voltage is pretty noisy/jumpy on Heltec WiFi Kit 32 V3
*/
#include <stdint.h>

void BATTERY_init();
uint32_t BATTERY_getCurrentVoltage();
uint32_t BATTERY_getCurrentVoltageStable();

#endif
