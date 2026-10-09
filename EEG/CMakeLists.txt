cmake_minimum_required(VERSION 4.2)
project(EEG)

set(CMAKE_CXX_STANDARD 17)

# library
add_subdirectory(libperun32)

# app
add_subdirectory(perun32-agent)