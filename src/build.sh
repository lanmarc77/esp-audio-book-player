#!/bin/sh

export IDF_COMPONENT_MANAGER=0
export ADF_PATH=/opt/esp-adf
. ~/esp/esp-idf/export.sh
idf.py clean
idf.py build
