cmake_minimum_required(VERSION 2.8)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)
include_directories(${ONT_SDK_ROOT}/include)
include_directories(${ONT_SDK_ROOT}/protocols/base)
include_directories(${ONT_SDK_ROOT}/sample/videosample/onvif)
include_directories(${ONT_SDK_ROOT}/sample/videosample/mp4v2/include
					${ONT_SDK_ROOT}/sample/videosample/live/BasicUsageEnvironment/include
					${ONT_SDK_ROOT}/sample/videosample/live/groupsock/include
					${ONT_SDK_ROOT}/sample/videosample/live/UsageEnvironment/include )
	
include_directories(${ONT_SDK_ROOT}/protocols/video/ont)
include_directories(${ONT_SDK_ROOT}/protocols/video/ont/cJSON)


if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)


if(_ONVIF)
    add_subdirectory(onvif)
    add_subdirectory(live)    
endif(_ONVIF)


if(_MP4V2)
    add_subdirectory(mp4v2)
endif(_MP4V2)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)


add_executable(sample_${VIDEO} ${SRC_LIST} )

if(WIN32 AND _ONVIF)
    message("set the ssl lib path")
    message(${CMAKE_BINARY_DIR}/../bin/lib)
    
    find_library(LIBEASY32MTD_LIBRARY NAMES easy32 libeay32MTd
             HINTS ${CMAKE_BINARY_DIR}/../bin/lib )
    list(APPEND LIBS ${LIBEASY32MTD_LIBRARY} )
    
    find_library(SSLEASY32MTD_LIBRARY NAMES ssleasy ssleay32MTd
             HINTS ${CMAKE_BINARY_DIR}/../bin/lib )
    list(APPEND LIBS ${SSLEASY32MTD_LIBRARY} )
endif(WIN32 AND _ONVIF)

if(_MP4V2)
    list(APPEND LIBS ${MP4V2})
endif(_MP4V2)

if(UNIX OR CYGWIN)
    add_definitions(-DONT_OS_POSIX)
    list(APPEND LIBS m)
    #used cpp
    list(APPEND LIBS stdc++)
    if(ARM-LINUX)
        list(APPEND LIBS libssl.a) 
        list(APPEND LIBS libcrypto.a)    
    else(ARM-LINUX)
        list(APPEND LIBS ssl) 
        list(APPEND LIBS crypto)      
    endif(ARM-LINUX)
    
    list(APPEND LIBS dl)
    list(APPEND LIBS pthread)
endif(UNIX OR CYGWIN)


target_link_libraries(sample_${VIDEO} ${MQTT} ${ONVIF} ${VIDEO} ${LIVERTSP} ${LIBS})
