/**
 * CpuMacOS.cpp — macOS implementation of SystemInfo.
 *
 * Data source: host_statistics(HOST_CPU_LOAD_INFO) via Mach kernel API.
 *
 * host_cpu_load_info_data_t provides cpu_ticks[4], indexed by:
 *   CPU_STATE_USER   = 0  — user-space time
 *   CPU_STATE_SYSTEM = 1  — kernel time
 *   CPU_STATE_IDLE   = 2  — idle time
 *   CPU_STATE_NICE   = 3  — niced user time
 *
 * These are cumulative clock ticks (natural Mach clock units) since boot,
 * summed across ALL logical cores — same as Linux /proc/stat "cpu" line.
 *
 * No iowait on macOS:
 *   Darwin does not expose iowait as a separate CPU state. Idle covers all
 *   time the CPU was not scheduled, including I/O waits at the scheduler level.
 *
 * Multi-core normalisation:
 *   host_statistics() aggregates ticks across all online CPUs automatically.
 *   The delta ratio is already the system-wide average; no per-core division.
 *
 * Alternative API — host_processor_info():
 *   Gives per-core breakdown but allocates a Mach port array each call
 *   (vm_deallocate required). Not suitable for the hot path. Use if you need
 *   per-core heat maps in a future MetricsCollector module.
 *
 * Linking: requires no extra frameworks beyond libSystem (linked by default).
 *   The -framework CoreFoundation is NOT needed for this call.
 */

#include "SystemInfo.hpp"
#include <mach/mach.h>
#include <new>
#include <unistd.h>

namespace profiler {

class MacOSSystemInfo final : public SystemInfo {
public:
    int getCoreCount() noexcept override {
        return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }

    bool readCpuSample(CpuSample& out) noexcept override {
        host_cpu_load_info_data_t info{};
        mach_msg_type_number_t    count = HOST_CPU_LOAD_INFO_COUNT;

        const kern_return_t kr = host_statistics(
            mach_host_self(),
            HOST_CPU_LOAD_INFO,
            reinterpret_cast<host_info_t>(&info),
            &count);

        if (kr != KERN_SUCCESS) return false;

        const uint64_t user   = info.cpu_ticks[CPU_STATE_USER];
        const uint64_t system = info.cpu_ticks[CPU_STATE_SYSTEM];
        const uint64_t idle   = info.cpu_ticks[CPU_STATE_IDLE];
        const uint64_t nice   = info.cpu_ticks[CPU_STATE_NICE];

        out.idle  = idle;
        out.total = user + system + idle + nice;
        return true;
    }
};

SystemInfo* SystemInfo::create() noexcept {
    return new (std::nothrow) MacOSSystemInfo();
}

} // namespace profiler
