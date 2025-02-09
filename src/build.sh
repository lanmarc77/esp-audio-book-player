#!/bin/sh

export IDF_COMPONENT_MANAGER=0
export ADF_PATH=/opt/esp-adf
. ~/esp/esp-idf/export.sh
cmake -S . -B ./build
cd ./build
make -j 12
#idf.py clean
#idf.py build
