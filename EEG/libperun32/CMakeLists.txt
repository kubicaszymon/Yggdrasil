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

add_library(libperun32 STATIC
        include/libperun32.h
        source/libperun32.cpp
)

target_link_libraries(libperun32 PRIVATE usb-1.0)

