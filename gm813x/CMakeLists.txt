cmake_minimum_required(VERSION 2.8)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

SET(EXECUTABLE_OUTPUT_PATH ${ONT_SDK_ROOT}/bin)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

SET(CMAKE_SYSTEM_NAME Linux)

include_directories(${ONT_SDK_ROOT}/include)
include_directories(${ONT_SDK_ROOT}/src/cJSON)
include_directories(${ONT_SDK_ROOT}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_executable(sample_test ${INC_LIST}	${SRC_LIST} $<TARGET_OBJECTS:${PLATFORM}_o>)

find_library(GM_LIBRARY NAMES gm HINTS ${CMAKE_CURRENT_SOURCE_DIR}/lib)
#find_library(RTMP_LIBRARY NAMES rtmp HINTS ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(sample_test pthread)
#target_link_libraries(sample_test ${RTMP_LIBRARY})
#target_link_libraries(sample_test ssl)
#target_link_libraries(sample_test crypto)
target_link_libraries(sample_test dl)
target_link_libraries(sample_test ${GM_LIBRARY})

list(APPEND LIBS m)

target_link_libraries(sample_test ${LIBS} ${VIDEO} )
