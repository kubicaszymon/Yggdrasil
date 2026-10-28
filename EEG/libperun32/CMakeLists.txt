add_library(libperun32 STATIC
        include/libperun32.h
        source/libperun32.cpp
)

target_link_libraries(libperun32 PRIVATE usb-1.0)

