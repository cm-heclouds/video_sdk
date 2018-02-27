cmake_minimum_required(VERSION 2.8)

project(test_sdk CXX)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${ONT_SDK_ROOT}/include
                    ${ONT_SDK_ROOT}/src)
SET(EXECUTABLE_OUTPUT_PATH ${ONT_SDK_ROOT}/bin)

if(WIN32)
    SET(LIBS ws2_32)
endif(WIN32)

if(WIN32)
    message("rsa lib path")
    message(${CMAKE_BINARY_DIR}/../lib/win)
    find_library(RSALIB NAMES mbedTLS
             HINTS ${CMAKE_BINARY_DIR}/../lib/win )
    list(APPEND LIBS ${RSALIB} )
endif(WIN32)

if(UNIX OR CYGWIN)
	find_library(RSALIB2 NAMES mbedcrypto
            HINTS ${ONT_SDK_ROOT}/lib/linux )
    list(APPEND LIBS ${RSALIB2} )
	
	list(APPEND LIBS m)
    list(APPEND LIBS dl)
    list(APPEND LIBS pthread)
endif(UNIX OR CYGWIN)



aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_executable(
    test_sdk
    ${SRC_LIST}
    $<TARGET_OBJECTS:${PLATFORM}_o>
	
)


target_link_libraries(test_sdk ${VIDEO} ${LIBS} )



