cmake_minimum_required(VERSION 2.8)

project(video_sdk )

set(CMAKE_DEBUG_POSTFIX "d")

if(NOT ONT_SOCK_SEND_BUF_SIZE)
    set(ONT_SOCK_SEND_BUF_SIZE 4096)
endif()

if(NOT ONT_SOCK_RECV_BUF_SIZE)
    set(ONT_SOCK_RECV_BUF_SIZE 2048)
endif()
if(NOT ONT_SERVER_ADDRESS)
    set(ONT_SERVER_ADDRESS "183.230.40.42")
endif()
if(NOT ONT_SERVER_PORT)
    set(ONT_SERVER_PORT 9101)
endif()

if(NOT _STATIC)
    if(WIN32)
        set(_STATIC 1)
    elseif(UNIX OR CYGWIN)
        option(_STATIC "static library" 1)
    endif()
endif()

if(NOT _SAMPLE)
    option(_SAMPLE "sample" 0)
endif()

if(NOT _ONVIF)
    option(_ONVIF "onvif" 0)
endif()

if(NOT _MP4V2)
    option(_MP4V2 "mp4v2" 0)
endif()

if(NOT _MONVIF)
    option(_MONVIF "multi thread onvif gateway" 0)
endif()

if(NOT _TEST)
    option(_TEST "sdk test" 0)
endif()

if(WIN32)
    add_definitions(-DONT_OS_WIN)
    add_definitions(-DWIN32_LEAN_AND_MEAN)

    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4819")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /DONT_DEBUG")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4819")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DONT_DEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    endif(MSVC)

elseif(UNIX OR CYGWIN)
    add_definitions(-DONT_OS_POSIX)

    if(CMAKE_COMPILER_IS_GNUCC)
        #use open source code not support c89
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -fpermissive")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb3")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")
        
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb3")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath,.")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,.")
    endif(CMAKE_COMPILER_IS_GNUCC)
	
    string(TOLOWER "${CMAKE_BUILD_TYPE}" __DEBUG)
    string(COMPARE EQUAL "${__DEBUG}" "debug" ___DEBUG)
    if(___DEBUG)
        add_definitions(-DONT_DEBUG)
    endif(___DEBUG)
endif(WIN32)

set(ONT_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${ONT_SDK_ROOT}/include)


add_definitions(-DNO_CRYPTO)
add_definitions(-DONT_VIDEO_PROTOCOL)

configure_file(${ONT_SDK_ROOT}/include/config.h.in ${ONT_SDK_ROOT}/include/config.h)

if(_STATIC)
    set(PLATFORM "${CMAKE_SYSTEM_NAME}_s")
	set(VIDEO     "video_s")
    set(MP4V2     "mp4v2_s")
    set(ONVIF     "onvif_s")
    set(LIVERTSP  "livertsp_s")
else()
    set(PLATFORM "${CMAKE_SYSTEM_NAME}_s")
	set(VIDEO     "video")
    set(MP4V2     "mp4v2")
    set(ONVIF     "onvif")
    set(LIVERTSP  "livertsp")   
endif(_STATIC)

# platforms
add_subdirectory(platforms)

# protocols
add_subdirectory(src)

# tests
if(_TEST)
add_subdirectory(test)
endif(_TEST)

# samples
if(_SAMPLE)
add_subdirectory(sample)
endif(_SAMPLE)

if(_MONVIF)
add_subdirectory(monvif)
add_subdirectory(sample/videosample/live)
add_subdirectory(sample/videosample/onvif)
endif(_MONVIF)


