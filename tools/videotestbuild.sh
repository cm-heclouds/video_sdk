#/bin/sh
cd ..
mkdir video_test_build
cd video_test_build
cmake .. -DCMAKE_BUILD_TYPE=Debug  -D_STATIC=1 -D_TEST=1
make -j4