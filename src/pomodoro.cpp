#include "pomodoro.hpp"

// Every body below is yours to write. The stubs exist only so the project
// compiles and the tests run RED instead of failing to build.
//
// Suggested order (each step turns a named group of tests green):
//   1. clamp_minutes, phase_after, duration_of
//   2. Timer ctor + observers
//   3. start / pause / toggle / reset
//   4. tick        <- the interesting one
//   5. skip / set_config

namespace pomo {

int clamp_minutes(double /*value*/, int /*max_value*/) {
  return 0;  // TODO
}

Phase phase_after(Phase /*current*/, int /*completed*/, int /*rounds*/) {
  return Phase::Focus;  // TODO
}

Duration duration_of(const Config& /*cfg*/, Phase /*p*/) {
  return Duration{0};  // TODO
}

Timer::Timer(Config cfg) : cfg_(cfg) {
  // TODO: a fresh timer sits at the top of a focus block, stopped.
}

Phase Timer::phase() const noexcept { return phase_; }

int Timer::completed() const noexcept { return completed_; }

bool Timer::running() const noexcept { return running_; }

Duration Timer::remaining() const noexcept {
  return Duration{0};  // TODO
}

const Config& Timer::config() const noexcept { return cfg_; }

void Timer::start(TimePoint /*now*/) {
  // TODO
}

void Timer::pause(TimePoint /*now*/) {
  // TODO
}

void Timer::toggle(TimePoint /*now*/) {
  // TODO
}

void Timer::reset() {
  // TODO
}

void Timer::skip(TimePoint /*now*/) {
  // TODO
}

int Timer::tick(TimePoint /*now*/) {
  return 0;  // TODO
}

void Timer::set_config(const Config& /*cfg*/, TimePoint /*now*/) {
  // TODO
}

}  // namespace pomo
