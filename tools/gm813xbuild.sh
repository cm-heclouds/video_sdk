#!/bin/sh
# you may need to run "sudo apt-get/yum install lib32ncurses5 lib32z1" first
#      for cross compiler of arm-unkown-xxx on x64 linux
# and do link libmpfr.so.4 for ccl, namely set LD_LIBRARY_PATH for libmpfr.so.4

SDK_ROOT=../../
BUILD_TYPE=Debug #Release
BUILD_DIR=video_gm813x_build_arm
GM813X_CROSS_COMPILER=g813x-arm.txt

cd $SDK_ROOT
mkdir $BUILD_DIR
cd    $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_DIR -D_GM813X=1 -DCMAKE_TOOLCHAIN_FILE=$GM813X_CROSS_COMPILER -DARM-LINUX=1
make -j4

