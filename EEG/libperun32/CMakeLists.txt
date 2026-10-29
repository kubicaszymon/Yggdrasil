add_library(libperun32 STATIC
        include/libperun32.h
        source/libperun32.cpp
)

include(FetchContent)
FetchContent_Declare(
        libusb
        GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
        GIT_TAG bea6567b63796ab27c1c33257d230284fb2d8316
)
FetchContent_MakeAvailable(libusb)

target_link_libraries(libperun32 PRIVATE usb-1.0)

