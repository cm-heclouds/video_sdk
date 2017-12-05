cmake_minimum_required(VERSION 2.8)

PROJECT(live CXX C)

set (ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment/include
    ${CMAKE_CURRENT_SOURCE_DIR}/groupsock/include
    ${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/include
    ${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment/include
)

if(MSVC)
    list(APPEND INC_LIST ${INC_TEMP})
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment/include/*.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/groupsock/include/*.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/include/*.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment/include/*.hh
    )
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

if (WIN32)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif(WIN32)

add_definitions(-DOMIT_REGISTER_HANDLING)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/groupsock SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/liveMedia SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment SRC_LIST)


if(_STATIC)
    add_library(${LIVERTSP} STATIC
       ${SRC_LIST}
    )
else(_STATIC)
    add_library(${LIVERTSP} SHARED
       ${SRC_LIST}
    )
endif(_STATIC)