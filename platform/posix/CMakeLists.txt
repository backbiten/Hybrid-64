# platform/posix/CMakeLists.txt — Linux/POSIX adapter and CLI

# -----------------------------------------------------------------------
# Adapter library
# -----------------------------------------------------------------------
add_library(hybrid64_posix STATIC
    src/posix_drive.c
)

target_include_directories(hybrid64_posix
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# PUBLIC so that consumers of hybrid64_posix also see core headers.
target_link_libraries(hybrid64_posix
    PUBLIC hybrid64_core
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hybrid64_posix PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# -----------------------------------------------------------------------
# CLI executable
# -----------------------------------------------------------------------
add_executable(hybrid64
    tools/hybrid64.c
)

# hybrid64_posix already exposes hybrid64_core's include path transitively.
target_link_libraries(hybrid64
    PRIVATE hybrid64_posix
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hybrid64 PRIVATE
        -Wall -Wextra
    )
endif()

install(TARGETS hybrid64 RUNTIME DESTINATION bin)
