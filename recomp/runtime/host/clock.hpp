// The runtime's one clock: what the scheduler's waits and timers, the
// kernel's tick count and system time, the time-stamp counter, the
// display's vblanks and DirectSound's play position all read.
//
// Normally it is the host's steady clock. With virtual time (qilin_run
// --virtual-time) it is a count of its own that moves only as the guest
// does: when every guest thread waits, to the earliest time one of them
// wakes (the scheduler's idle processor), and by a fixed amount for work,
// at each preemption point lifted code passes and each rdtsc. A run then
// takes the same course every time, however fast the host is: scripted
// presses land where they did, and frames can be compared run to run.
#pragma once

#include <chrono>
#include <cstdint>

namespace qilin::host::clock {

using Clock = std::chrono::steady_clock;

// Switches to virtual time; before the runtime starts.
void use_virtual();
bool is_virtual();

// Now: the host's clock, or the virtual time (from where it began).
Clock::time_point now();

// Virtual time only: moves on to `t` (never back), or by a span of work.
void advance_to(Clock::time_point t);
void spend(std::chrono::nanoseconds d);

// The system time, 100 ns since 1601-01-01 UTC: the host's, or with
// virtual time a fixed start (2001-11-15, the console's launch) plus the
// virtual time elapsed, so the guest's dates don't vary either.
int64_t system_time();

// Virtual time's cost of work: a preemption point (a loop's iteration, a
// few dozen instructions at 733 MHz) and an rdtsc.
inline constexpr std::chrono::nanoseconds kPollCost{50}, kRdtscCost{20};

}  // namespace qilin::host::clock
