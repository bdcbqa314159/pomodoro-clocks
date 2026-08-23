#include "pomodoro.hpp"

#include <algorithm>
#include <cmath>

namespace pomo {

namespace {
Duration minutes_to_duration(int m) {
  return std::chrono::duration_cast<Duration>(std::chrono::minutes(m));
}
}  // namespace

int clamp_minutes(double value, int max_value) {
  const long rounded = std::lround(value);  // half away from zero
  return static_cast<int>(std::clamp<long>(rounded, 1, max_value));
}

Phase phase_after(Phase current, int completed, int rounds) {
  if (current != Phase::Focus) {
    return Phase::Focus;
  }
  // rounds <= 0 would be a division by zero; treat it as "never a long break".
  return (rounds > 0 && completed % rounds == 0) ? Phase::LongBreak : Phase::ShortBreak;
}

Duration duration_of(const Config& cfg, Phase p) {
  switch (p) {
    case Phase::Focus:
      return minutes_to_duration(cfg.focus_min);
    case Phase::ShortBreak:
      return minutes_to_duration(cfg.short_min);
    case Phase::LongBreak:
      return minutes_to_duration(cfg.long_min);
  }
  return Duration{0};  // unreachable for a valid Phase; keeps -Wreturn-type quiet
}

Timer::Timer(Config cfg) : cfg_(cfg), remaining_(duration_of(cfg, Phase::Focus)) {}

Phase Timer::phase() const noexcept { return phase_; }

int Timer::completed() const noexcept { return completed_; }

bool Timer::running() const noexcept { return running_; }

Duration Timer::remaining() const noexcept { return remaining_; }

const Config& Timer::config() const noexcept { return cfg_; }

void Timer::start(TimePoint now) {
  if (running_) {
    return;  // idempotent: must not push the deadline out
  }
  deadline_ = now + remaining_;
  running_ = true;
}

void Timer::pause(TimePoint now) {
  if (!running_) {
    return;
  }
  remaining_ = std::max(std::chrono::duration_cast<Duration>(deadline_ - now), Duration{0});
  running_ = false;
}

void Timer::toggle(TimePoint now) {
  if (running_) {
    pause(now);
  } else {
    start(now);
  }
}

void Timer::reset() {
  running_ = false;
  remaining_ = duration_of(cfg_, phase_);
}

void Timer::skip(TimePoint now) {
  if (phase_ == Phase::Focus) {
    ++completed_;
  }
  phase_ = phase_after(phase_, completed_, cfg_.rounds);
  remaining_ = duration_of(cfg_, phase_);
  if (running_) {
    deadline_ = now + remaining_;
  }
}

int Timer::tick(TimePoint now) {
  if (!running_) {
    return 0;
  }

  int transitions = 0;
  while (now >= deadline_) {
    if (phase_ == Phase::Focus) {
      ++completed_;
    }
    phase_ = phase_after(phase_, completed_, cfg_.rounds);
    ++transitions;

    const Duration next = duration_of(cfg_, phase_);
    // A zero-length phase would never move the deadline past `now` — that is an
    // infinite loop, not a fast timer. Config comes from a plain struct, so this
    // is reachable without going through clamp_minutes(); stop instead of hang.
    if (next <= Duration{0}) {
      running_ = false;
      remaining_ = Duration{0};
      return transitions;
    }
    // Measured from the previous deadline, never from `now` — that is what keeps
    // the overshoot from being silently forgiven once per phase.
    deadline_ += next;
  }

  remaining_ = std::max(std::chrono::duration_cast<Duration>(deadline_ - now), Duration{0});
  return transitions;
}

void Timer::set_config(const Config& cfg) {
  const Duration before = duration_of(cfg_, phase_);
  cfg_ = cfg;
  if (duration_of(cfg_, phase_) != before) {
    reset();  // the phase you are sitting in changed length: retime it
  }
}

}  // namespace pomo
