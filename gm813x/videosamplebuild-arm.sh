#/bin/sh
cd ..
mkdir g813x_build_arm
cd g813x_build_arm
cmake .. -DCMAKE_BUILD_TYPE=Debug -D_GM813X=1 -DCMAKE_TOOLCHAIN_FILE=../g813x-arm.txt  -DARM-LINUX=1
make -j4