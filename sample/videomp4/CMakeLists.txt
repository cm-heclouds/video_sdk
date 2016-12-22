cmake_minimum_required(VERSION 2.8)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)
include_directories(${ONT_SDK_ROOT}/include)
include_directories(${ONT_SDK_ROOT}/protocols/base)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)



if(UNIX OR CYGWIN)
    add_definitions(-DONT_OS_POSIX)
    list(APPEND LIBS m) 
    list(APPEND LIBS stdc++)
endif(UNIX OR CYGWIN)


add_executable(sample_${VIDEO} ${SRC_LIST} 
)
target_link_libraries(sample_${VIDEO} ${MQTT} ${VIDEO} ${MP4V2}  ${LIBS})
