cmake_minimum_required(VERSION 2.8)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

SET(EXECUTABLE_OUTPUT_PATH ${ONT_SDK_ROOT}/bin)

include_directories(${ONT_SDK_ROOT}/sample)
include_directories(${ONT_SDK_ROOT}/sample/onvif)
include_directories(${ONT_SDK_ROOT}/sample/mp4v2/include
					${ONT_SDK_ROOT}/sample/live/BasicUsageEnvironment/include
					${ONT_SDK_ROOT}/sample/live/groupsock/include
					${ONT_SDK_ROOT}/sample/live/UsageEnvironment/include)
include_directories(${ONT_SDK_ROOT}/src/cJSON)
include_directories(${ONT_SDK_ROOT}/src/librtmp)
include_directories(${ONT_SDK_ROOT}/src/mbedtls-2.6.0/include)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

if(_ONVIF)
    add_definitions(-DONT_VIDEO_ONVIF_SUPPORTED)
    add_subdirectory(onvif)
    add_subdirectory(live)    
	list(APPEND LIBS ${ONVIF} ${LIVERTSP} )
endif(_ONVIF)

if(_MP4V2)
    add_definitions(-DONT_VIDEO_RVOD_MP4SUPPORTED)
	if(_STATIC)
		add_definitions(-DMP4V2_USE_STATIC_LIB)
	endif(_STATIC)
	add_subdirectory(mp4v2)
endif(_MP4V2)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

if(WIN32)
    if (_ONVIF OR _DPROTOCOL_SECURITY_OPENSSL)
		message("set the ssl lib path")
		message(${CMAKE_BINARY_DIR}/../lib/win)
		find_library(LIBEASY32MTD_LIBRARY NAMES easy32 libeay32MTd
				 HINTS ${CMAKE_BINARY_DIR}/../lib/win )
		list(APPEND LIBS ${LIBEASY32MTD_LIBRARY} )
		
		find_library(SSLEASY32MTD_LIBRARY NAMES ssleasy ssleay32MTd
				 HINTS ${CMAKE_BINARY_DIR}/../lib/win )
		list(APPEND LIBS ${SSLEASY32MTD_LIBRARY} )
	endif()

    if (_PROTOCOL_SECURITY_MBEDTLS)
		find_library(RSALIB NAMES mbedTLS
					HINTS ${CMAKE_BINARY_DIR}/../lib/win )
		list(APPEND LIBS ${RSALIB} )
	endif()
	list(APPEND LIBS ws2_32 )
endif()

if(_MP4V2)
    list(APPEND LIBS ${MP4V2} )
endif(_MP4V2)

if(UNIX OR CYGWIN)
    add_definitions(-DONT_OS_POSIX)
    list(APPEND LIBS m)
    #used cpp
    list(APPEND LIBS stdc++)

    if(ARM-LINUX)
	    if(_ONVIF OR _DPROTOCOL_SECURITY_OPENSSL)
			list(APPEND LIBS libssl.a) 
			list(APPEND LIBS libcrypto.a)    
		endif()
		
		if(_PROTOCOL_SECURITY_MBEDTLS)
			find_library(RSALIB NAMES mbedcrypto
				 HINTS ${CMAKE_BINARY_DIR}/../lib/arm )
		    list(APPEND LIBS ${RSALIB} )
		endif()
    else()
	    if(_ONVIF OR _DPROTOCOL_SECURITY_OPENSSL )
			list(APPEND LIBS ssl) 
			list(APPEND LIBS crypto)
		endif()
		
	    if(_PROTOCOL_SECURITY_MBEDTLS)
	        find_library(RSALIB NAMES mbedcrypto
		         HINTS ${CMAKE_BINARY_DIR}/../lib/linux )
		    list(APPEND LIBS ${RSALIB} )
	    endif()		
    endif(ARM-LINUX)
			 
    list(APPEND LIBS dl)
    list(APPEND LIBS pthread)
endif(UNIX OR CYGWIN)

add_executable(
    sample_${VIDEO}
    ${SRC_LIST}
    $<TARGET_OBJECTS:${PLATFORM}_o>)

target_link_libraries(sample_${VIDEO} ${VIDEO}  ${LIBS})