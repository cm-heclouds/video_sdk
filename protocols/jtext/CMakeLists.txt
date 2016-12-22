cmake_minimum_required(VERSION 2.8)

set (ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(
    ${ONT_SDK_ROOT}/include
    ${ONT_SDK_ROOT}/protocols/base
)

if(MSVC)
    file(GLOB_RECURSE INC_TEMP FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    list(APPEND INC_LIST ${INC_TEMP})
endif(MSVC)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_library(${JTEXT}_o OBJECT ${INC_LIST} ${SRC_LIST})

if(_STATIC)
    add_library(${JTEXT} STATIC
        $<TARGET_OBJECTS:${PLATFORM}_o>
        $<TARGET_OBJECTS:${BASE}_o>
        $<TARGET_OBJECTS:${JTEXT}_o>
    )
else(_STATIC)
    add_library(${JTEXT} SHARED
        $<TARGET_OBJECTS:${PLATFORM}_o>
        $<TARGET_OBJECTS:${BASE}_o>
        $<TARGET_OBJECTS:${JTEXT}_o>
    )
endif(_STATIC)

target_link_libraries(${JTEXT} ${LIBS})
