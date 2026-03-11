cmake_minimum_required(VERSION 3.16)
project(Hybrid64 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compile options
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ── HAL library ──────────────────────────────────────────────────────────────
add_library(hal STATIC
    software/hal/hal.c
)
target_include_directories(hal PUBLIC software/hal)

# ── CPU driver library ────────────────────────────────────────────────────────
add_library(cpu_driver STATIC
    software/drivers/cpu_driver.c
)
target_include_directories(cpu_driver PUBLIC software/drivers)
target_link_libraries(cpu_driver PUBLIC hal)

# ── Reference application ─────────────────────────────────────────────────────
add_executable(hybrid64
    software/apps/main.c
)
target_link_libraries(hybrid64 PRIVATE cpu_driver hal)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(test_hal
    software/hal/test_hal.c
)
target_link_libraries(test_hal PRIVATE hal)
add_test(NAME HalTests COMMAND test_hal)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS hybrid64 DESTINATION bin)
install(FILES software/hal/hal.h software/drivers/cpu_driver.h
        DESTINATION include/hybrid64)
