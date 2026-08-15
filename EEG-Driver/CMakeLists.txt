cmake_minimum_required(VERSION 4.2)
project(EEG_Driver)

set(CMAKE_CXX_STANDARD 17)

add_executable(EEG_Driver main.cpp)

if(WIN32)
    include(FetchContent)

    set(LIBUSB_INSTALL_TARGETS OFF)
    FetchContent_Declare(
            libusb_cmake
            GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
            GIT_TAG        v1.0.30-0
    )
    FetchContent_MakeAvailable(libusb_cmake)
endif()

if(WIN32 OR UNIX)
    target_link_libraries(EEG_Driver PRIVATE usb-1.0)
endif()