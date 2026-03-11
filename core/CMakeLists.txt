# core/CMakeLists.txt — freestanding C core library
#
# Intentionally compiled WITHOUT any POSIX or OS-specific flags so that
# the core can also target bare-metal MCUs.

add_library(hybrid64_core STATIC
    src/drive.c
)

target_include_directories(hybrid64_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hybrid64_core PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()
