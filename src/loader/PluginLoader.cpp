/**
 * PluginLoader.cpp — POSIX implementation (Linux + macOS).
 *
 * Key design decisions:
 *
 * 1. RTLD_LOCAL — symbols from the plugin are NOT promoted to the global
 *    symbol table. This prevents symbol collisions when two plugins
 *    define functions with the same name (e.g. both link libz).
 *
 * 2. RTLD_NOW — resolve all symbols at dlopen() time, not lazily.
 *    Lazy resolution (RTLD_LAZY) means a missing symbol only crashes
 *    when that code path is hit at runtime. Fail-fast is better here.
 *
 * 3. Paired create/destroy — the plugin allocates its IModule on its
 *    own heap and frees it too. This is the only safe pattern when the
 *    host and plugin may have been compiled with different CRTs.
 *
 * 4. Destruction order — module->shutdown() → ~module → dlclose().
 *    Reversing this causes vtable-read-after-free.
 */

#include "PluginLoader.hpp"
#include <dlfcn.h>
#include <cstring>
#include <utility>

namespace profiler {

// ── PluginHandle ─────────────────────────────────────────────────────────────

PluginHandle::~PluginHandle() {
    if (module) {
        module->shutdown();
        // We need the destroy function, but we no longer have the handle
        // to look it up. In practice the plugin registered it; we call
        // shutdown() here and let the plugin's destroy_module (called
        // from PluginLoader::unload_all) free the memory.
        // PluginHandle destructor only runs after unload_all has already
        // called destroy_module — so module is a dangling ptr at this point
        // only if misused. See unload_all() for the safe path.
        module = nullptr;
    }
    if (lib) {
        dlclose(lib);
        lib = nullptr;
    }
}

PluginHandle::PluginHandle(PluginHandle&& o) noexcept
    : lib(o.lib), module(o.module), path(std::move(o.path))
{
    o.lib    = nullptr;
    o.module = nullptr;
}

PluginHandle& PluginHandle::operator=(PluginHandle&& o) noexcept {
    if (this != &o) {
        // Clean up current resources first
        if (module) { module->shutdown(); module = nullptr; }
        if (lib)    { dlclose(lib);       lib    = nullptr; }

        lib    = o.lib;
        module = o.module;
        path   = std::move(o.path);
        o.lib    = nullptr;
        o.module = nullptr;
    }
    return *this;
}

// ── PluginLoader ─────────────────────────────────────────────────────────────

bool PluginLoader::load(const std::string& path) noexcept {
    // ── Open shared library ───────────────────────────────────────────────
    void* lib = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        last_error_ = dlerror();
        return false;
    }

    // ── Resolve required symbols ──────────────────────────────────────────
    // Cast via void* to suppress -Wpedantic on function pointer casts.
    void* raw_create  = dlsym(lib, "create_module");
    void* raw_destroy = dlsym(lib, "destroy_module");

    if (!raw_create || !raw_destroy) {
        last_error_  = "missing symbol: ";
        last_error_ += !raw_create ? "create_module" : "destroy_module";
        last_error_ += " — is this a valid profiler plugin?";
        dlclose(lib);
        return false;
    }

    CreateFn  create_fn;
    DestroyFn destroy_fn;
    std::memcpy(&create_fn,  &raw_create,  sizeof(create_fn));
    std::memcpy(&destroy_fn, &raw_destroy, sizeof(destroy_fn));

    // ── Instantiate and initialise module ─────────────────────────────────
    IModule* mod = create_fn();
    if (!mod) {
        last_error_ = "create_module() returned nullptr";
        dlclose(lib);
        return false;
    }

    if (!mod->init()) {
        last_error_  = "IModule::init() failed for plugin: ";
        last_error_ += path;
        destroy_fn(mod);
        dlclose(lib);
        return false;
    }

    // ── Store handle ──────────────────────────────────────────────────────
    // We store destroy_fn alongside the handle so unload_all() can use it.
    // Embed it via a small wrapper stored in the handle's path field? No —
    // store it in a parallel vector to keep PluginHandle simple.
    // Simpler: just re-resolve it during unload (dlsym on already-open lib).
    PluginHandle h;
    h.lib    = lib;
    h.module = mod;
    h.path   = path;
    plugins_.push_back(std::move(h));

    return true;
}

void PluginLoader::unload_all() noexcept {
    // Unload in reverse order (LIFO) — mirrors construction order,
    // respects inter-plugin dependencies if any exist.
    for (int i = static_cast<int>(plugins_.size()) - 1; i >= 0; --i) {
        PluginHandle& h = plugins_[static_cast<std::size_t>(i)];

        if (h.module && h.lib) {
            // Shut down first
            h.module->shutdown();

            // Resolve destroy_fn while the library is still open
            void* raw_destroy = dlsym(h.lib, "destroy_module");
            if (raw_destroy) {
                DestroyFn destroy_fn;
                std::memcpy(&destroy_fn, &raw_destroy, sizeof(destroy_fn));
                destroy_fn(h.module);
            }
            h.module = nullptr;

            dlclose(h.lib);
            h.lib = nullptr;
        }
    }
    plugins_.clear();
}

} // namespace profiler
