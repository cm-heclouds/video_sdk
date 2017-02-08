cmake_minimum_required(VERSION 2.8)

project(video C)
# base
add_subdirectory(ont)

if(_ONVIF)
    add_subdirectory(onvif)
    add_subdirectory(live)    
endif(_ONVIF)


if(_MP4V2)
    add_subdirectory(mp4v2)
endif(_MP4V2)