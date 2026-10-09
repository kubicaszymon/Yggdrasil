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
        template.cpp
)
