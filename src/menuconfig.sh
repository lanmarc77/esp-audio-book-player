#!/bin/sh

export ADF_PATH=/opt/esp-adf
. ~/esp/esp-idf/export.sh
idf.py menuconfig
