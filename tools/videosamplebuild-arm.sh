#!/bin/sh
#tips: you may need to run "sudo apt-get/yum install lib32ncurses5 lib32z1" first
#      for cross compiler of arm-linux on x64 linux

VIDEO_SDK_ROOT=../
BUILD_TYPE=Release #Debug
BUILD_DIR=video_sample_build_on_arm_linux
ARM_LINUX_CROSS_COMPILER=linux-arm.txt

cd $VIDEO_SDK_ROOT
mkdir $BUILD_DIR
cd    $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE  -D_MP4V2=1 -D_ONVIF=1 -D_SAMPLE=1 -DCMAKE_TOOLCHAIN_FILE=$ARM_LINUX_CROSS_COMPILER  -DARM-LINUX=1
make -j4
