cmake_minimum_required(VERSION 3.16)

project(Hybrid64
    VERSION     0.1.0
    DESCRIPTION "Hybrid-64 portable block-device abstraction library"
    LANGUAGES   C
)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

# Core is always built.
add_subdirectory(core)

# POSIX adapter and CLI are built only on Linux (require /proc/mounts semantics).
if(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(platform/posix)
endif()
