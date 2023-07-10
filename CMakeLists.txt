cmake_minimum_required(VERSION 3.25)
project(modern_beep C)

set(CMAKE_C_STANDARD 11)

add_executable(beep beep.c)
target_link_libraries(beep soundio m)
