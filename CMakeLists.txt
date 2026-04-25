cmake_minimum_required(VERSION 3.16)
project(profiler_cpu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

# ── Platform source selection ─────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    set(PLATFORM_SRC src/platform/CpuLinux.cpp)
    message(STATUS "Platform: Linux   -> CpuLinux.cpp")
elseif(APPLE)
    set(PLATFORM_SRC src/platform/CpuMacOS.cpp)
    message(STATUS "Platform: macOS   -> CpuMacOS.cpp")
elseif(WIN32)
    set(PLATFORM_SRC src/platform/CpuWindows.cpp)
    message(STATUS "Platform: Windows -> CpuWindows.cpp")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ── Core static library ───────────────────────────────────────────────────────
add_library(cpu_module STATIC
    src/CpuModule.cpp
    ${PLATFORM_SRC}
)
target_include_directories(cpu_module PUBLIC include)

# Required when cpu_module is linked into a shared library (cpu_plugin).
# Without this, the linker rejects non-PIC code in .so/.dylib on x86-64.
set_target_properties(cpu_module PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    target_link_libraries(cpu_module PRIVATE "-framework CoreFoundation")
endif()

# ── Plugin shared library (.so / .dylib) ──────────────────────────────────────
add_library(cpu_plugin SHARED
    src/cpu_plugin.cpp
)
target_link_libraries(cpu_plugin PRIVATE cpu_module)
target_include_directories(cpu_plugin PUBLIC include)

set_target_properties(cpu_plugin PROPERTIES
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# ── PluginLoader static library ───────────────────────────────────────────────
add_library(plugin_loader STATIC
    src/loader/PluginLoader.cpp
)
target_include_directories(plugin_loader PUBLIC include)

if(UNIX AND NOT APPLE)
    target_link_libraries(plugin_loader PRIVATE dl)
endif()

# ── Threads ───────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── Test binaries ─────────────────────────────────────────────────────────────
add_executable(cpu_test test/cpu_test.cpp)
target_link_libraries(cpu_test PRIVATE cpu_module Threads::Threads)

add_executable(loader_test test/loader_test.cpp)
target_link_libraries(loader_test PRIVATE plugin_loader Threads::Threads)

target_compile_definitions(loader_test PRIVATE
    DEFAULT_PLUGIN_PATH="$<TARGET_FILE:cpu_plugin>"
)
