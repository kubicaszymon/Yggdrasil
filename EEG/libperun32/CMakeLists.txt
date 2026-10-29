add_library(libperun32 STATIC
        include/libperun32.h
        source/libperun32.cpp
)

include(FetchContent)
FetchContent_Declare(
        libusb
        GIT_REPOSITORY https://github.com/libusb/libusb.git
        GIT_TAG 6a1aff0b39c724657fd92b2d2bf82eb532344570
)
FetchContent_MakeAvailable(libusb)

target_link_libraries(libperun32 PRIVATE usb-1.0)

