cmake_minimum_required(VERSION 2.8)

PROJECT(platforms C)

if(WIN32)
    add_subdirectory(win)
elseif(UNIX OR CYGWIN)
    add_subdirectory(posix)
endif(WIN32)