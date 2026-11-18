add_executable(perun32-agent main.cpp)

include(FetchContent)
FetchContent_Declare(
        liblsl
        GIT_REPOSITORY https://github.com/sccn/liblsl.git
        GIT_TAG v1.18.0.b7
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(liblsl)

target_link_libraries(perun32-agent PRIVATE libperun32 LSL::lsl)