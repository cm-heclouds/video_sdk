#/bin/sh
cd ..
mkdir gm813x_build_arm
cd gm813x_build_arm
cmake .. -DCMAKE_BUILD_TYPE=Release -D_GM813X=1 -DCMAKE_TOOLCHAIN_FILE=../g813x-arm.txt  -DARM-LINUX=1
make -j4
