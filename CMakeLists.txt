cmake_minimum_required(VERSION 3.13)
project(networks C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_EXE_LINKER_FLAGS "-pthread")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")
include_directories(.)

add_executable(networks
        app.c
        app.h
        )
