#!/bin/sh

export IDF_COMPONENT_MANAGER=0
export ADF_PATH=/opt/esp-adf
. ~/esp/esp-idf/export.sh
idf_tools.py install-python-env
