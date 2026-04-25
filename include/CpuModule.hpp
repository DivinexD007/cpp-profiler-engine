#pragma once
#include "IModule.hpp"
#include "SystemInfo.hpp"
#include <memory>

namespace profiler {

/**
 * CpuModule - IModule implementation for system-wide CPU usage.
 *
 * Output JSON (fixed-size, stack-allocated buffer, no heap on hot path):
 *   {"usage_percent":12.34,"cores":8,"timestamp_ns":1713000000000000000}
 *
 * Thread safety: NOT thread-safe. Use one instance per thread or
 * provide external locking. The delta state (prev_sample_) is mutable
 * and unsynchronised.
 *
 * First call to collect() after init() primes the baseline sample and
 * returns 0.00% — this is intentional and correct, not a bug.
 */
class CpuModule final : public IModule {
public:
    CpuModule() noexcept = default;
    ~CpuModule() override;

    // Non-copyable, non-movable (owns OS resources via sysinfo_)
    CpuModule(const CpuModule&)            = delete;
    CpuModule& operator=(const CpuModule&) = delete;

    std::string_view name()     const noexcept override;
    bool             init()           noexcept override;
    void             shutdown()       noexcept override;
    const char*      collect()        noexcept override;

private:
    // 128 bytes is enough for the fixed JSON schema.
    // Verified: max length ~72 chars with realistic values.
    static constexpr std::size_t kJsonBuf = 128;

    std::unique_ptr<SystemInfo> sysinfo_;
    CpuSample                   prev_{};
    bool                        primed_  = false; // true after first real sample
    bool                        ready_   = false; // true after init()
    int                         cores_   = 0;
    char                        buf_[kJsonBuf]{};
};

} // namespace profiler
