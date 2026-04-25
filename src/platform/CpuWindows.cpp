/**
 * CpuWindows.cpp — Windows implementation of SystemInfo.
 *
 * Data source: GetSystemTimes() — kernel32, no special privileges needed.
 *
 * FILETIME semantics:
 *   GetSystemTimes(&idle, &kernel, &user)
 *   - idle:   time all processors spent idle
 *   - kernel: time all processors spent in kernel mode  ← INCLUDES idle
 *   - user:   time all processors spent in user mode
 *
 * CRITICAL: kernel includes idle. This is a Windows API quirk that
 * trips up most naive implementations. The formula must be:
 *
 *   total  = kernel + user          (not kernel - idle + user)
 *   active = (kernel - idle) + user
 *   idle   = idle
 *   usage% = active / total * 100
 *           = (1 - idle / total) * 100
 *
 * Adding idle separately to get "total active" would subtract idle
 * twice. The formula above is correct and consistent with Task Manager.
 *
 * FILETIME to uint64_t:
 *   FILETIME is a 64-bit count of 100-nanosecond intervals split into
 *   two 32-bit DWORDs. Shift-and-OR reconstructs the 64-bit value.
 *   Direct ULARGE_INTEGER cast would also work but requires alignment.
 *
 * Multi-core normalisation:
 *   GetSystemTimes() sums ticks across all processors automatically.
 *   The ratio already represents the average across all cores.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "SystemInfo.hpp"
#include <windows.h>
#include <new>

namespace profiler {

static inline uint64_t ftToU64(const FILETIME& ft) noexcept {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32)
         |  static_cast<uint64_t>(ft.dwLowDateTime);
}

class WindowsSystemInfo final : public SystemInfo {
public:
    int getCoreCount() noexcept override {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        return static_cast<int>(si.dwNumberOfProcessors);
    }

    bool readCpuSample(CpuSample& out) noexcept override {
        FILETIME ft_idle{}, ft_kernel{}, ft_user{};
        if (!GetSystemTimes(&ft_idle, &ft_kernel, &ft_user)) return false;

        const uint64_t idle   = ftToU64(ft_idle);
        const uint64_t kernel = ftToU64(ft_kernel); // includes idle
        const uint64_t user   = ftToU64(ft_user);

        out.idle  = idle;
        out.total = kernel + user;   // kernel already includes idle: correct denominator
        return true;
    }
};

SystemInfo* SystemInfo::create() noexcept {
    return new (std::nothrow) WindowsSystemInfo();
}

} // namespace profiler
