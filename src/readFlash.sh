#!/bin/bash

export IDF_COMPONENT_MANAGER=0
export ADF_PATH=/opt/esp-adf
. ~/esp/esp-idf/export.sh
~/.espressif/python_env/idf5.1_py3.11_env/bin/esptool.py -b 460800 --port /dev/ttyUSB0 read_flash 0 0x800000 flash_all8mb.bin
