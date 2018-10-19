cmake_minimum_required(VERSION 2.8.8)

set(LIBRARY_OUTPUT_PATH ${ONT_SDK_ROOT}/lib)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

if (WIN32)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif(WIN32)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_library(${PLATFORM}_o OBJECT ${INC_LIST} ${SRC_LIST})

if(_STATIC)
    add_library(platform_s STATIC
       ${SRC_LIST}
    )
else(_STATIC)
    add_library(platform SHARED
       ${SRC_LIST}
    )
endif(_STATIC)

