cmake_minimum_required(VERSION 2.8)

project(sample_ont_sdk C)

if(WIN32)
    add_subdirectory(win)
elseif(UNIX OR CYGWIN)
    add_subdirectory(posix)
endif(WIN32)