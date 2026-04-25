#pragma once
#include <string_view>

namespace profiler {

/**
 * IModule - Base interface for all profiling plugin modules.
 *
 * Thread safety: Single-threaded by default unless the implementation
 * explicitly documents otherwise. Callers must externally synchronize
 * if concurrent collect() calls are needed.
 *
 * No exceptions cross module boundaries. All error states are
 * communicated via return values or error JSON payloads.
 */
class IModule {
public:
    virtual ~IModule() = default;

    // Stable ASCII identifier for this module (e.g. "cpu", "memory").
    virtual std::string_view name() const noexcept = 0;

    // Allocate platform resources. Returns false on failure.
    // Idempotent: calling init() twice is safe.
    virtual bool init() noexcept = 0;

    // Release all resources. Safe to call on an uninitialised module.
    virtual void shutdown() noexcept = 0;

    // Sample the metric. Returns a null-terminated JSON string valid
    // until the next call to collect() on this instance.
    // Must not throw. Returns an error JSON payload on failure.
    virtual const char* collect() noexcept = 0;
};

} // namespace profiler
