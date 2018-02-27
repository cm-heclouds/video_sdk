#/bin/sh
cd ..
mkdir video_sample_build
cd video_sample_build
cmake .. -DCMAKE_BUILD_TYPE=Debug  -D_MP4V2=1 -D_ONVIF=1 -D_SAMPLE=1
make -j4