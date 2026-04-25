/**
 * CpuLinux.cpp — Linux implementation of SystemInfo.
 *
 * Data source: /proc/stat, first line ("cpu" aggregate across all cores).
 *
 * /proc/stat line format:
 *   cpu  user nice system idle iowait irq softirq steal guest guest_nice
 *        ^^^^      ^^^^^^ ^^^^  ^^^^^
 *        All values are cumulative jiffies since boot.
 *
 * idle accounting:
 *   idle  = idle + iowait
 *   Rationale: iowait is time the CPU was idle waiting for I/O to complete.
 *   Including it gives a more accurate picture of "doing nothing useful".
 *   Some monitoring tools exclude iowait; we include it (Linux convention).
 *
 * total accounting:
 *   total = user + nice + system + idle + iowait + irq + softirq + steal
 *   We exclude guest and guest_nice because they are already counted inside
 *   user/nice — adding them again would double-count and inflate total.
 *
 * Multi-core normalisation:
 *   /proc/stat "cpu" is the SUM across all cores. The delta ratio is
 *   therefore already averaged; no per-core division needed.
 *
 * Performance:
 *   fopen/fscanf on /proc/stat is a kernel pseudo-file: no disk I/O,
 *   no page faults. Typical latency on Linux: 5-30 µs.
 */

#include "SystemInfo.hpp"
#include <cstdio>
#include <unistd.h>
#include <new>

namespace profiler {

class LinuxSystemInfo final : public SystemInfo {
public:
    int getCoreCount() noexcept override {
        // _SC_NPROCESSORS_ONLN counts logical cores currently online.
        // Returns -1 on failure; caller in CpuModule clamps to 1.
        return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }

    bool readCpuSample(CpuSample& out) noexcept override {
        FILE* f = std::fopen("/proc/stat", "r");
        if (!f) return false;

        // Use unsigned long long explicitly to match %llu format specifier.
        // Avoids undefined behaviour from uint64_t being unsigned long on
        // some 64-bit Linux toolchains.
        unsigned long long user, nice, system, idle,
                           iowait, irq, softirq, steal;
        char label[8];

        const int n = std::fscanf(f,
            "%7s %llu %llu %llu %llu %llu %llu %llu %llu",
            label, &user, &nice, &system,
            &idle, &iowait, &irq, &softirq, &steal);

        std::fclose(f);

        // Need all 8 counter fields (label + 8 values = 9 tokens).
        if (n < 9) return false;

        out.idle  = idle + iowait;
        out.total = user + nice + system + idle + iowait + irq + softirq + steal;
        return true;
    }
};

// ── Factory (only one translation unit defines this per build) ──────────────
SystemInfo* SystemInfo::create() noexcept {
    return new (std::nothrow) LinuxSystemInfo();
}

} // namespace profiler
