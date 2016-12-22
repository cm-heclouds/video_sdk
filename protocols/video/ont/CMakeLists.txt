cmake_minimum_required(VERSION 2.8)


project(ont_video C)
set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

set(LIBRARY_OUTPUT_PATH ${ONT_SDK_ROOT}/lib)

include_directories(
    ${ONT_SDK_ROOT}
    ${ONT_SDK_ROOT}/include
    ${ONT_SDK_ROOT}/base
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ONT_SDK_ROOT}/protocols/video/mp4v2/include
)

if(MSVC)
    list(APPEND INC_LIST ${INC_TEMP})
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/librtmp/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

    
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/librtmp SRC_LIST)


if(_STATIC)
    add_library(${VIDEO} STATIC
       ${SRC_LIST}
    )
else(_STATIC)
    add_library(${VIDEO} SHARED
       ${SRC_LIST}
    )
endif(_STATIC)