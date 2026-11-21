// The Xbox's time-stamp counter, from the host's clock: what lifted `rdtsc`
// reads (XAPI's QueryPerformanceCounter, the engine's timers).
#include <chrono>

#include "host.hpp"

namespace qilin::host {

namespace {
constexpr uint64_t kTscHz = 733333333;  // the console's processor clock
}

uint64_t tsc_now() {
    static const auto start = std::chrono::steady_clock::now();
    const uint64_t ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    return ns / 1000000000u * kTscHz + ns % 1000000000u * kTscHz / 1000000000u;
}

}  // namespace qilin::host

namespace qilin::rt {
uint64_t rdtsc(uint32_t) { return host::tsc_now(); }
}  // namespace qilin::rt
