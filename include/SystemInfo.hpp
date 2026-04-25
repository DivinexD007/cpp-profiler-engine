#pragma once
#include <cstdint>
#include <cstddef> // std::nothrow_t

namespace profiler {

/**
 * Raw CPU counter snapshot.
 *
 * idle:  ticks spent idle (+ iowait on Linux)
 * total: ticks across all states (user + nice + system + idle + ...)
 *
 * Usage formula:
 *   delta_idle  = curr.idle  - prev.idle
 *   delta_total = curr.total - prev.total
 *   cpu%        = (1 - delta_idle / delta_total) * 100
 *
 * Unsigned subtraction handles counter wrap-around naturally on all
 * platforms as long as the actual elapsed ticks fit in uint64_t, which
 * is guaranteed for any realistic uptime.
 */
struct CpuSample {
    uint64_t idle  = 0;
    uint64_t total = 0;
};

/**
 * SystemInfo - OS-level CPU data source.
 *
 * One concrete implementation per platform, compiled via CMake
 * platform selection. The factory SystemInfo::create() is defined
 * in exactly one of: CpuLinux.cpp, CpuWindows.cpp, CpuMacOS.cpp.
 */
class SystemInfo {
public:
    virtual ~SystemInfo() = default;

    // Number of logical (hyper-threaded) cores online.
    virtual int getCoreCount() noexcept = 0;

    // Fill out with a new CPU counter snapshot.
    // Returns false if the OS call fails.
    virtual bool readCpuSample(CpuSample& out) noexcept = 0;

    // Platform factory. Returns nullptr on allocation failure.
    // Caller takes ownership (store in unique_ptr).
    static SystemInfo* create() noexcept;
};

} // namespace profiler
