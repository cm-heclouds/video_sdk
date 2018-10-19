#!/bin/sh
VIDEO_SDK_ROOT=../../
BUILD_TYPE=Release #Debug
BUIDL_DIR=video_sample_build_on_linux

cd $VIDEO_SDK_ROOT
mkdir $BUIDL_DIR
cd    $BUIDL_DIR
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -D_ONVIF=1 -D_SAMPLE=1
make -j4
