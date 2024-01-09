cmake_minimum_required(VERSION 3.18)
project(lclClient C)

set(CMAKE_C_STANDARD 11)

add_executable(lclClient main.c)

target_link_libraries(lclClient pthread)
