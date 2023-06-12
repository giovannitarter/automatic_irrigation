#!/bin/bash

pushd /root/project

if [ -z "$CVERSION" ];
then
    echo "missing CVERSION in ENV"
    exit 1
fi

if [ -z "$STASSID" ];
then
    echo "missing STASSID in ENV"
    exit 1
fi

if [ -z "$STAPSK" ];
then
    echo "missing STAPSK in ENV"
    exit 1
fi

#if [ -z "$SRVNAME" ];
#then
#    SRVNAME="espcamsrv1"
#fi
#
#if [ -z "$SRVPORT" ];
#then
#    SRVPORT="8085"
#fi
#
#if [ -z "$SLEEP_TIME" ];
#then
#    SLEEP_TIME="1"
#fi

export PLATFORMIO_BUILD_FLAGS=" \
-DSTASSID=\\\"$STASSID\\\" \
-DSTAPSK=\\\"$STAPSK\\\" \
-DCVERSION=$CVERSION \
-DSLEEP_TIME=$SLEEP_TIME \
"

platformio pkg install
platformio run

cp .pio/build/esp32cam/firmware.bin /output/
#/root/create_manifest.py /output/manifest

