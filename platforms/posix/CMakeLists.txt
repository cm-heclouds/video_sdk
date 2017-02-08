cmake_minimum_required(VERSION 2.8)

PROJECT(platform C)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_library(${PLATFORM}_o OBJECT ${INC_LIST} ${SRC_LIST})
