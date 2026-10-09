add_executable(EEG main.cpp)

if(WIN32 OR UNIX)
    target_link_libraries(EEG PRIVATE usb-1.0)
endif()