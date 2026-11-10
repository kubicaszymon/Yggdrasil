add_library(libperun32 STATIC
        api/libperun32.h
        source/libperun32.cpp
        include/definitions.h
        include/commands.h
        source/perun32.cpp
        include/perun32.h
        source/IsoStream.cpp
        include/IsoStream.h
)

include(FetchContent)
FetchContent_Declare(
        libusb
        GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
        GIT_TAG bea6567b63796ab27c1c33257d230284fb2d8316
)
FetchContent_MakeAvailable(libusb)

target_link_libraries(libperun32 PRIVATE usb-1.0)

target_include_directories(libperun32
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/api
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

