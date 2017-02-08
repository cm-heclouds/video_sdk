cmake_minimum_required(VERSION 2.8)

project(protocols C)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LIBRARY_OUTPUT_PATH ${ONT_SDK_ROOT}/lib)

# base
add_subdirectory(base)

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


if(_VIDEO)
    add_subdirectory(video)
endif(_VIDEO)