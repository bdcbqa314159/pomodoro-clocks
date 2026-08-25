#include "pomodoro.hpp"

#include <gtest/gtest.h>

#include <ostream>

using namespace std::chrono_literals;
using pomo::Config;
using pomo::Duration;
using pomo::Phase;
using pomo::TimePoint;
using pomo::Timer;

namespace pomo {
// Readable failure messages instead of raw bytes.
std::ostream &operator<<(std::ostream &os, Phase p) {
  switch (p) {
  case Phase::Focus:
    return os << "Focus";
  case Phase::ShortBreak:
    return os << "ShortBreak";
  case Phase::LongBreak:
    return os << "LongBreak";
  }
  return os << "?";
}
} // namespace pomo

namespace {

// A fixed origin for fake time. steady_clock's epoch is unspecified, which is
// exactly why the timer must never read the clock itself.
const TimePoint t0{};

Duration mins(int m) { return std::chrono::duration_cast<Duration>(std::chrono::minutes(m)); }

// ---------------------------------------------------------------------------
// Task 1 — pure functions
// ---------------------------------------------------------------------------

TEST(ClampMinutes, ClampsIntoRange) {
  EXPECT_EQ(pomo::clamp_minutes(25, 180), 25);
  EXPECT_EQ(pomo::clamp_minutes(0, 180), 1);
  EXPECT_EQ(pomo::clamp_minutes(-5, 180), 1);
  EXPECT_EQ(pomo::clamp_minutes(999, 180), 180);
  EXPECT_EQ(pomo::clamp_minutes(180, 180), 180);
  EXPECT_EQ(pomo::clamp_minutes(1, 180), 1);
}

TEST(ClampMinutes, RoundsToNearest) {
  EXPECT_EQ(pomo::clamp_minutes(25.4, 180), 25);
  EXPECT_EQ(pomo::clamp_minutes(25.6, 180), 26);
  EXPECT_EQ(pomo::clamp_minutes(2.5, 180), 3); // half away from zero
}

TEST(PhaseAfter, FocusGoesToShortBreakMidSet) {
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 1, 4), Phase::ShortBreak);
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 2, 4), Phase::ShortBreak);
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 3, 4), Phase::ShortBreak);
}

TEST(PhaseAfter, FocusGoesToLongBreakOnRoundBoundary) {
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 4, 4), Phase::LongBreak);
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 8, 4), Phase::LongBreak);
  EXPECT_EQ(pomo::phase_after(Phase::Focus, 2, 2), Phase::LongBreak);
}

TEST(PhaseAfter, EveryBreakGoesBackToFocus) {
  EXPECT_EQ(pomo::phase_after(Phase::ShortBreak, 1, 4), Phase::Focus);
  EXPECT_EQ(pomo::phase_after(Phase::LongBreak, 4, 4), Phase::Focus);
}

TEST(DurationOf, ReadsTheConfig) {
  Config cfg; // 25 / 5 / 15
  EXPECT_EQ(pomo::duration_of(cfg, Phase::Focus), mins(25));
  EXPECT_EQ(pomo::duration_of(cfg, Phase::ShortBreak), mins(5));
  EXPECT_EQ(pomo::duration_of(cfg, Phase::LongBreak), mins(15));

  Config custom{50, 10, 30, 3};
  EXPECT_EQ(pomo::duration_of(custom, Phase::Focus), mins(50));
  EXPECT_EQ(pomo::duration_of(custom, Phase::LongBreak), mins(30));
}

// ---------------------------------------------------------------------------
// Task 2 — Timer
// ---------------------------------------------------------------------------

TEST(TimerInit, StartsStoppedAtTopOfFocus) {
  Timer t;
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completed(), 0);
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(25));
}

TEST(TimerInit, HonoursACustomConfig) {
  Timer t{Config{50, 10, 30, 3}};
  EXPECT_EQ(t.remaining(), mins(50));
  EXPECT_EQ(t.config().rounds, 3);
}

TEST(TimerRun, StartThenTickCountsDown) {
  Timer t;
  t.start(t0);
  EXPECT_TRUE(t.running());
  EXPECT_EQ(t.remaining(), mins(25)); // start alone consumes nothing

  EXPECT_EQ(t.tick(t0 + mins(10)), 0);
  EXPECT_EQ(t.remaining(), mins(15));
  EXPECT_EQ(t.phase(), Phase::Focus);
}

TEST(TimerRun, StartIsIdempotent) {
  Timer t;
  t.start(t0);
  t.start(t0 + mins(5)); // must not push the deadline out
  EXPECT_EQ(t.tick(t0 + mins(10)), 0);
  EXPECT_EQ(t.remaining(), mins(15));
}

TEST(TimerRun, PauseFreezesTheCountdown) {
  Timer t;
  t.start(t0);
  t.pause(t0 + mins(10));
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(15));

  EXPECT_EQ(t.tick(t0 + mins(90)), 0); // time passes, paused timer ignores it
  EXPECT_EQ(t.remaining(), mins(15));

  t.start(t0 + mins(90));
  t.tick(t0 + mins(95));
  EXPECT_EQ(t.remaining(), mins(10));
}

TEST(TimerRun, PauseOnAStoppedTimerIsANoOp) {
  Timer t;
  t.pause(t0 + mins(10));
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(25));
}

TEST(TimerRun, ToggleFlipsRunningState) {
  Timer t;
  t.toggle(t0);
  EXPECT_TRUE(t.running());
  t.toggle(t0 + mins(10));
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(15));
}

TEST(TimerRun, ResetReturnsToTopOfPhaseAndStops) {
  Timer t;
  t.start(t0);
  t.tick(t0 + mins(10));
  t.reset();
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(25));
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completed(), 0);
}

TEST(TimerRun, RemainingNeverGoesNegative) {
  Timer t;
  t.start(t0);
  t.pause(t0 + mins(999));
  EXPECT_GE(t.remaining(), Duration{0});
}

// --- tick: the phase machine ---

TEST(TimerTick, FocusEndsExactlyOnTheDeadline) {
  Timer t;
  t.start(t0);
  EXPECT_EQ(t.tick(t0 + mins(25)), 1);
  EXPECT_EQ(t.phase(), Phase::ShortBreak);
  EXPECT_EQ(t.completed(), 1);
  EXPECT_EQ(t.remaining(), mins(5));
  EXPECT_TRUE(t.running()); // auto-continues
}

TEST(TimerTick, DoesNotDrift) {
  // Overshoot by 500ms: the break must be 500ms short, not a full 5 minutes.
  // This only holds if each deadline is measured from the previous deadline.
  Timer t;
  t.start(t0);
  EXPECT_EQ(t.tick(t0 + mins(25) + 500ms), 1);
  EXPECT_EQ(t.remaining(), mins(5) - 500ms);
}

TEST(TimerTick, RollsThroughSeveralPhasesInOneJump) {
  // Laptop asleep: focus(25) -> short(5) -> focus, at minute 31.
  Timer t;
  t.start(t0);
  EXPECT_EQ(t.tick(t0 + mins(31)), 2);
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completed(), 1);
  EXPECT_EQ(t.remaining(), mins(24)); // deadline at minute 55
}

TEST(TimerTick, ReachesTheLongBreakAfterFourFocusBlocks) {
  // 25,30,55,60,85,90,115 -> 7 transitions, 4 focus blocks done.
  Timer t;
  t.start(t0);
  EXPECT_EQ(t.tick(t0 + mins(115)), 7);
  EXPECT_EQ(t.phase(), Phase::LongBreak);
  EXPECT_EQ(t.completed(), 4);
  EXPECT_EQ(t.remaining(), mins(15));
}

TEST(TimerTick, StoppedTimerNeverAdvances) {
  Timer t;
  EXPECT_EQ(t.tick(t0 + mins(500)), 0);
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completed(), 0);
}

// --- skip ---

TEST(TimerSkip, EndsFocusEarlyAndKeepsRunning) {
  Timer t;
  t.start(t0);
  t.skip(t0 + mins(3));
  EXPECT_EQ(t.phase(), Phase::ShortBreak);
  EXPECT_EQ(t.completed(), 1);
  EXPECT_TRUE(t.running());
  EXPECT_EQ(t.remaining(), mins(5));

  t.tick(t0 + mins(5)); // 2 min into the 5 min break
  EXPECT_EQ(t.remaining(), mins(3));
}

TEST(TimerSkip, OnAStoppedTimerLeavesItStopped) {
  Timer t;
  t.skip(t0);
  EXPECT_EQ(t.phase(), Phase::ShortBreak);
  EXPECT_FALSE(t.running());
  EXPECT_EQ(t.remaining(), mins(5));
}

TEST(TimerSkip, BreakSkipsBackToFocusWithoutCrediting) {
  Timer t;
  t.skip(t0); // focus -> short break, completed = 1
  t.skip(t0); // break  -> focus, still 1
  EXPECT_EQ(t.phase(), Phase::Focus);
  EXPECT_EQ(t.completed(), 1);
}

// --- set_config ---

TEST(SetConfig, RetimesTheCurrentPhaseWhenItsLengthChanged) {
  Timer t;
  t.start(t0);
  t.tick(t0 + mins(10));
  t.set_config(Config{50, 5, 15, 4});
  EXPECT_EQ(t.remaining(), mins(50));
  EXPECT_FALSE(t.running()); // retiming stops the clock, like reset()
}

TEST(SetConfig, LeavesTheCountdownAloneWhenAnotherPhaseChanged) {
  Timer t;
  t.start(t0);
  t.tick(t0 + mins(10));
  t.set_config(Config{25, 10, 15, 4}); // only short_min moved
  EXPECT_EQ(t.remaining(), mins(15));
  EXPECT_TRUE(t.running());
  EXPECT_EQ(t.config().short_min, 10);

  t.tick(t0 + mins(25)); // focus still ends on its original deadline
  EXPECT_EQ(t.phase(), Phase::ShortBreak);
  EXPECT_EQ(t.remaining(), mins(10)); // new break length applies
}

TEST(SetConfig, RoundsChangeIsNotARetime) {
  Timer t;
  t.start(t0);
  t.tick(t0 + mins(10));
  t.set_config(Config{25, 5, 15, 2});
  EXPECT_TRUE(t.running());
  EXPECT_EQ(t.remaining(), mins(15));
  EXPECT_EQ(t.config().rounds, 2);
}

} // namespace
