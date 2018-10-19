#!/bin/sh
VIDEO_SDK_ROOT=../
BUILD_TYPE=Release #Debug
BUILD_DIR=video_sample_build_on_linux

cd $VIDEO_SDK_ROOT
mkdir $BUILD_DIR
cd    $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -D_MP4V2=1 -D_SAMPLE=1
make -j4
