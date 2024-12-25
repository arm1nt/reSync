cmake_minimum_required(VERSION 3.22)
project(reSync C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(.)

add_compile_definitions(LOG DEBUG)
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-pedantic;-Wall;-g>")

add_executable(reSync
        client/reSync.c
        )

add_executable(reSyncServer
        server/reSyncd.c
        server/util/util.h server/util/mem_util.h)
