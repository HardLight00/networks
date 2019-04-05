cmake_minimum_required(VERSION 3.13)
project(networks C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(networks
        app.c
        app.h
        torrent.c
        torrent.h)
