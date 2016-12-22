cmake_minimum_required(VERSION 2.8)

project(sample_ont_sdk C)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
SET(EXECUTABLE_OUTPUT_PATH ${ONT_SDK_ROOT}/bin)

if(WIN32)
    SET(LIBS ws2_32)
endif(WIN32)

if(_EDP)
    add_subdirectory(edp)
endif(_EDP)

if(_JTEXT)
    add_subdirectory(jtext)
endif(_JTEXT)

if(_MODBUS)
    add_subdirectory(modbus)
endif(_MODBUS)

if(_MQTT)
    add_subdirectory(mqtt)
endif(_MQTT)

if(_MP4V2)
    add_subdirectory(videomp4)
endif(_MP4V2)


if(_ONVIF)
    add_subdirectory(videoonvif)
endif(_ONVIF)
