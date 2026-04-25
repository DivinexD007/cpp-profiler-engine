#include "CpuModule.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <thread>
#include <vector>

// ── Helpers ──────────────────────────────────────────────────────────────────

static void separator(const char* title) {
    std::printf("\n─────────────────────────────────────────\n");
    std::printf("  %s\n", title);
    std::printf("─────────────────────────────────────────\n");
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    profiler::CpuModule mod;

    // ── Init ─────────────────────────────────────────────────────────────────
    separator("Init");
    if (!mod.init()) {
        std::fprintf(stderr, "FATAL: CpuModule::init() failed\n");
        return 1;
    }
    std::printf("Module     : %.*s\n",
        static_cast<int>(mod.name().size()), mod.name().data());

    // ── Validation: 10 samples at 500 ms intervals ───────────────────────────
    //
    // Expected behaviour:
    //   Sample 0 → 0.00%  (first-call prime, intentional)
    //   Samples 1-9 → realistic usage matching your system monitor
    //
    // Cross-check: open htop/Activity Monitor/Task Manager alongside.
    separator("Validation  (10 samples × 500 ms)");
    std::printf("  %-4s  %-56s\n", "#", "JSON");
    std::printf("  %-4s  %-56s\n", "----", "--------");

    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const char* json = mod.collect();
        std::printf("  %-4d  %s\n", i, json);
    }

    // ── Benchmark: 10,000 rapid calls ────────────────────────────────────────
    //
    // NOTE: Rapid calls without sleep will show 0% (no time elapsed between
    // samples). That is correct — we are benchmarking collection overhead,
    // not CPU activity. The clock tick counters simply haven't advanced.
    separator("Benchmark  (10,000 calls, no sleep)");

    constexpr int kRuns = 10'000;
    std::vector<long long> ns;
    ns.reserve(kRuns);

    for (int i = 0; i < kRuns; ++i) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        mod.collect();
        const auto t1 = std::chrono::high_resolution_clock::now();
        ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    std::sort(ns.begin(), ns.end());

    const double mean_ns =
        static_cast<double>(std::accumulate(ns.begin(), ns.end(), 0LL)) / kRuns;
    const long long p50_ns = ns[kRuns / 2];
    const long long p95_ns = ns[static_cast<std::size_t>(kRuns * 0.95)];
    const long long p99_ns = ns[static_cast<std::size_t>(kRuns * 0.99)];
    const long long max_ns = ns.back();

    std::printf("\n");
    std::printf("  Mean    : %7.0f ns  (%6.2f µs)\n", mean_ns, mean_ns / 1e3);
    std::printf("  P50     : %7lld ns  (%6.2f µs)\n", p50_ns,  p50_ns  / 1e3);
    std::printf("  P95     : %7lld ns  (%6.2f µs)\n", p95_ns,  p95_ns  / 1e3);
    std::printf("  P99     : %7lld ns  (%6.2f µs)\n", p99_ns,  p99_ns  / 1e3);
    std::printf("  Max     : %7lld ns  (%6.2f µs)\n", max_ns,  max_ns  / 1e3);

    const bool pass = (mean_ns / 1e3) < 200.0;
    std::printf("\n  Budget (mean < 200 µs): %s\n", pass ? "PASS ✓" : "FAIL ✗");

    // ── Edge case: double init is safe ────────────────────────────────────────
    separator("Edge Cases");
    const bool reinit = mod.init();
    std::printf("  Double init()     : %s\n", reinit ? "ok" : "FAIL");

    mod.shutdown();
    const char* after_shutdown = mod.collect();
    const bool has_error = (std::strstr(after_shutdown, "error") != nullptr);
    std::printf("  collect() after shutdown returns error JSON: %s\n",
        has_error ? "ok" : "FAIL");
    std::printf("  JSON: %s\n", after_shutdown);

    separator("Done");
    return 0;
}
