/**
 * cpu_plugin.cpp — Shared library entry point for CpuModule.
 *
 * This file is the ONLY thing that changes when building CpuModule as a
 * plugin (.so/.dylib) vs a static library. Everything else is identical.
 *
 * The two exported C functions are the plugin contract:
 *
 *   create_module()  — allocate a new CpuModule on the plugin's heap
 *   destroy_module() — free it through the same heap
 *
 * Why extern "C"?
 *   C++ name mangling is compiler and ABI specific. A symbol exported as
 *   profiler::CpuModule::create() from GCC looks nothing like it does from
 *   Clang. extern "C" gives a stable, predictable symbol name that dlsym()
 *   can find regardless of which compiler built the host vs the plugin.
 *
 * Why a paired destroy?
 *   On macOS and Linux with a single shared libc this matters less, but on
 *   Windows different CRT versions have separate heaps. Always freeing
 *   through the allocator that allocated is the only safe universal pattern.
 */

#include "CpuModule.hpp"
#include <new>

// Explicitly mark these as default visibility even under -fvisibility=hidden.
// extern "C" alone is not sufficient — the visibility attribute is required
// when CXX_VISIBILITY_PRESET is set to hidden in CMake.
#if defined(_WIN32)
#  define PLUGIN_EXPORT __declspec(dllexport)
#else
#  define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

PLUGIN_EXPORT profiler::IModule* create_module() {
    return new (std::nothrow) profiler::CpuModule();
}

PLUGIN_EXPORT void destroy_module(profiler::IModule* mod) {
    delete mod;
}

} // extern "C"
