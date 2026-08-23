#pragma once

#include <chrono>

// Pure timer core — no I/O, no terminal, no clock reads.
// Everything that needs "now" takes it as a parameter, so tests drive fake time.

namespace pomo {

enum class Phase { Focus, ShortBreak, LongBreak };

struct Config {
  int focus_min = 25;
  int short_min = 5;
  int long_min = 15;
  int rounds = 4;  // focus blocks before a long break
};

// Bounds every path into Config must respect: the settings screen, the config
// file parser, and anything else that sets a duration.
inline constexpr int kMaxMinutes = 180;
inline constexpr int kMaxRounds = 12;

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::milliseconds;

// ---------------------------------------------------------------------------
// Task 1 — pure functions
// ---------------------------------------------------------------------------

/// Round to nearest int, then clamp into [1, max_value].
int clamp_minutes(double value, int max_value);

/// Which phase follows `current`.
/// After a focus block: a long break when `completed` is a multiple of `rounds`,
/// otherwise a short break. After any break: focus.
/// `completed` counts focus blocks finished *including* the one that just ended.
Phase phase_after(Phase current, int completed, int rounds);

/// Configured length of `p`, as a Duration.
Duration duration_of(const Config& cfg, Phase p);

// ---------------------------------------------------------------------------
// Task 2 — the timer
// ---------------------------------------------------------------------------

class Timer {
 public:
  explicit Timer(Config cfg = Config{});

  // --- observers ---
  Phase phase() const noexcept;
  /// Focus blocks completed since construction.
  int completed() const noexcept;
  bool running() const noexcept;
  /// Time left in the current phase, as of the last tick()/start()/pause().
  /// Never negative.
  Duration remaining() const noexcept;
  const Config& config() const noexcept;

  // --- commands ---
  /// No-op if already running. Otherwise the phase ends at now + remaining().
  void start(TimePoint now);
  /// No-op if not running. Freezes remaining().
  void pause(TimePoint now);
  void toggle(TimePoint now);
  /// Back to the top of the current phase, stopped. Leaves phase() and
  /// completed() untouched.
  void reset();
  /// End the current phase early and move to the next one. Preserves running().
  void skip(TimePoint now);

  /// Advance the clock to `now`, rolling through every phase whose deadline has
  /// passed (a laptop asleep for two hours must not lose a beat, and must not
  /// drift — each new deadline is measured from the previous deadline, not from
  /// `now`). Returns how many phases ended. Returns 0 when not running.
  int tick(TimePoint now);

  /// Replace the configuration. If the *current* phase's length changed, retime
  /// the current phase (as reset() does, so the clock stops); otherwise leave the
  /// countdown running and untouched.
  void set_config(const Config& cfg);

 private:
  Config cfg_{};
  Phase phase_ = Phase::Focus;
  int completed_ = 0;
  bool running_ = false;
  Duration remaining_{};   // authoritative while paused
  TimePoint deadline_{};   // authoritative while running
};

}  // namespace pomo
