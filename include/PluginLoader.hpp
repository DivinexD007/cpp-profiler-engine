#pragma once
#include "IModule.hpp"
#include <string>
#include <vector>
#include <memory>

namespace profiler {

/**
 * PluginHandle - Owns one loaded shared library and its IModule instance.
 *
 * Destruction order matters:
 *   1. Call module->shutdown()
 *   2. Destroy the module object
 *   3. dlclose() the library
 *
 * If you close the library before destroying the module, the vtable
 * pointers become dangling — instant UB. This class enforces the
 * correct order via its destructor.
 */
struct PluginHandle {
    void*                      lib    = nullptr; // dlopen handle
    IModule*                   module = nullptr; // raw ptr — owned by lib
    std::string                path;             // for diagnostics

    PluginHandle() = default;
    ~PluginHandle();

    // Non-copyable — owns OS handle
    PluginHandle(const PluginHandle&)            = delete;
    PluginHandle& operator=(const PluginHandle&) = delete;

    // Movable
    PluginHandle(PluginHandle&& o) noexcept;
    PluginHandle& operator=(PluginHandle&& o) noexcept;
};

/**
 * PluginLoader - Discovers and loads profiler plugin shared libraries.
 *
 * Plugin contract (what each .so/.dylib must export):
 *
 *   extern "C" profiler::IModule* create_module();
 *   extern "C" void               destroy_module(profiler::IModule*);
 *
 * Using a paired destroy function (rather than delete) is critical:
 * the plugin and host may use different heap allocators, especially
 * across MSVC runtime boundaries on Windows. Always free through the
 * same allocator that allocated.
 *
 * Thread safety: NOT thread-safe. One loader per thread, or external lock.
 */
class PluginLoader {
public:
    using CreateFn  = IModule* (*)();
    using DestroyFn = void (*)(IModule*);

    PluginLoader()  = default;
    ~PluginLoader() = default;

    // Load a plugin from an absolute or relative path.
    // Calls create_module() and init() on the plugin.
    // Returns false and populates last_error() on any failure.
    bool load(const std::string& path) noexcept;

    // Unload all plugins in reverse load order (LIFO).
    void unload_all() noexcept;

    // Iterate loaded modules (e.g. to call collect() on all of them).
    const std::vector<PluginHandle>& plugins() const noexcept { return plugins_; }

    // Human-readable description of the last failure.
    const std::string& last_error() const noexcept { return last_error_; }

private:
    std::vector<PluginHandle> plugins_;
    std::string               last_error_;
};

} // namespace profiler
