// The runtime's clock (see clock.hpp).
#include "clock.hpp"

#include <atomic>

namespace qilin::host::clock {

namespace {
std::atomic<bool> g_virtual{false};
std::atomic<int64_t> g_ns{0};  // virtual time: nanoseconds since it began
const Clock::time_point g_base = Clock::now();
// 2001-11-15 00:00 UTC in 100 ns since 1601.
constexpr int64_t kVirtualStart = 116444736000000000LL + 1005782400LL * 10000000;
}  // namespace

void use_virtual() { g_virtual.store(true); }
bool is_virtual() { return g_virtual.load(std::memory_order_relaxed); }

Clock::time_point now() {
    if (!is_virtual()) return Clock::now();
    return g_base + std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(g_ns.load(std::memory_order_relaxed)));
}

void advance_to(Clock::time_point t) {
    const int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t - g_base).count();
    int64_t cur = g_ns.load(std::memory_order_relaxed);
    while (ns > cur && !g_ns.compare_exchange_weak(cur, ns, std::memory_order_relaxed)) {
    }
}

void spend(std::chrono::nanoseconds d) {
    if (is_virtual()) g_ns.fetch_add(d.count(), std::memory_order_relaxed);
}

int64_t system_time() {
    if (is_virtual()) return kVirtualStart + g_ns.load(std::memory_order_relaxed) / 100;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 100 +
           116444736000000000LL;
}

}  // namespace qilin::host::clock
