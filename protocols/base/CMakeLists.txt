cmake_minimum_required(VERSION 2.8)

set (ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(
    ${ONT_SDK_ROOT}/include
)

if(MSVC)
    list(APPEND INC_LIST ${INC_TEMP})
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/parser/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/parser SRC_LIST)

add_library(${BASE}_o OBJECT ${INC_LIST} ${SRC_LIST})