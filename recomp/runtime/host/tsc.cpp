// The Xbox's time-stamp counter, from the host's clock: what lifted `rdtsc`
// reads (XAPI's QueryPerformanceCounter, the engine's timers).
#include <chrono>

#include "clock.hpp"
#include "host.hpp"

namespace qilin::host {

namespace {
constexpr uint64_t kTscHz = 733333333;  // the console's processor clock
}

uint64_t tsc_now() {
    static const auto start = clock::now();
    const uint64_t ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count());
    return ns / 1000000000u * kTscHz + ns % 1000000000u * kTscHz / 1000000000u;
}

}  // namespace qilin::host

namespace qilin::rt {
// With virtual time each read is a little work, so a loop timing itself by
// the counter (XAPI's calibration against QueryPerformanceCounter) ends.
uint64_t rdtsc(uint32_t) {
    host::clock::spend(host::clock::kRdtscCost);
    return host::tsc_now();
}
}  // namespace qilin::rt
