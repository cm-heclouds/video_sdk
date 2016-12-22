cmake_minimum_required(VERSION 2.8)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${ONT_SDK_ROOT}/include)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_executable(sample_${MQTT} ${INC_LIST} ${SRC_LIST})

target_link_libraries(sample_${MQTT} ${MQTT} ${LIBS})
