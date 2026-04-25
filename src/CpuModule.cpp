#include "CpuModule.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

namespace profiler {

// ── Lifecycle ────────────────────────────────────────────────────────────────

CpuModule::~CpuModule() {
    shutdown();
}

std::string_view CpuModule::name() const noexcept {
    return "cpu";
}

bool CpuModule::init() noexcept {
    if (ready_) return true;

    sysinfo_.reset(SystemInfo::create());
    if (!sysinfo_) return false;

    cores_ = sysinfo_->getCoreCount();
    if (cores_ <= 0) cores_ = 1; // defensive fallback

    ready_  = true;
    primed_ = false; // first collect() will prime prev_
    return true;
}

void CpuModule::shutdown() noexcept {
    sysinfo_.reset();
    ready_  = false;
    primed_ = false;
    cores_  = 0;
}

// ── Hot path ─────────────────────────────────────────────────────────────────

const char* CpuModule::collect() noexcept {
    // ── Error states ─────────────────────────────────────────────────────────
    if (!ready_) {
        std::strncpy(buf_, R"({"error":"not_initialized"})", kJsonBuf - 1);
        buf_[kJsonBuf - 1] = '\0';
        return buf_;
    }

    CpuSample curr{};
    if (!sysinfo_->readCpuSample(curr)) {
        std::strncpy(buf_, R"({"error":"read_failed"})", kJsonBuf - 1);
        buf_[kJsonBuf - 1] = '\0';
        return buf_;
    }

    // ── First call: prime baseline, return 0% ────────────────────────────────
    // We deliberately return 0% rather than a garbage value.
    // The caller should discard the first sample or treat it as a warm-up.
    if (!primed_) {
        prev_   = curr;
        primed_ = true;

        const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::snprintf(buf_, kJsonBuf,
            R"({"usage_percent":0.00,"cores":%d,"timestamp_ns":%lld})",
            cores_, static_cast<long long>(now_ns));
        return buf_;
    }

    // ── Delta-based CPU usage calculation ────────────────────────────────────
    //
    // All arithmetic is on uint64_t: subtraction wraps correctly if a counter
    // overflows (two's-complement unsigned wrap is well-defined in C++).
    //
    // Guard: if delta_total == 0 the system clock stalled or we sampled twice
    // within one scheduler tick. Return 0% rather than divide-by-zero.
    //
    // Guard: delta_idle > delta_total should be impossible, but clamp to 0%
    // defensively (can happen on some VM hypervisors that lie about idle).

    const uint64_t delta_total = curr.total - prev_.total;
    const uint64_t delta_idle  = curr.idle  - prev_.idle;

    double usage = 0.0;
    if (delta_total > 0 && delta_idle <= delta_total) {
        usage = (1.0 - static_cast<double>(delta_idle) /
                       static_cast<double>(delta_total)) * 100.0;
    }

    prev_ = curr; // store for next delta

    // ── Timestamp ────────────────────────────────────────────────────────────
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // ── Serialise — no heap, stack buffer only ────────────────────────────────
    std::snprintf(buf_, kJsonBuf,
        R"({"usage_percent":%.2f,"cores":%d,"timestamp_ns":%lld})",
        usage, cores_, static_cast<long long>(now_ns));

    return buf_;
}

} // namespace profiler
