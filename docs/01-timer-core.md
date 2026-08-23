# Milestone 1 — the timer core

> **Status: done.** Bernardo asked me to implement it rather than drive it as an exercise, so
> `src/pomodoro.cpp` is written and all 26 tests pass. The doc stays as the design record — the
> Background and "the flaw this design exists to avoid" sections explain *why* the code looks
> the way it does, and the Stretch goals are still open.

## Context

`prototype/index.html` is a working pomodoro: SVG ring, chimes, notifications, settings.
We're rebuilding it as a C++ TUI. This milestone ports **only the logic** — no terminal, no
sound, no FTXUI. Getting the core right first means the TUI milestone is pure rendering.

Files:

| File | Owner |
|---|---|
| `src/pomodoro.hpp` | me — the interface, don't change it |
| `src/pomodoro.cpp` | **you** — every body |
| `tests/pomodoro_test.cpp` | me — the oracle, don't edit to pass |

## Background — the concept

**The whole design turns on one decision: the core never reads a clock.**

Every function that needs the current time takes a `TimePoint` parameter. That's *dependency
injection for time*, and it buys three things:

1. **Tests run instantly.** `t.tick(t0 + mins(115))` simulates two hours in a microsecond.
   A core that called `steady_clock::now()` internally could only be tested by sleeping.
2. **Determinism.** No flakiness from a test machine under load.
3. **It forces the drift question into the open** (see below).

Two C++ points you'll meet:

- **`std::chrono` is a type-safe unit system.** `minutes`, `seconds`, `milliseconds` are
  distinct types. Widening conversions (minutes → milliseconds) are implicit; narrowing ones
  (milliseconds → minutes) need `duration_cast`, because they lose information. A `TimePoint`
  minus a `TimePoint` is a `Duration`; a `TimePoint` plus a `Duration` is a `TimePoint`;
  `TimePoint + TimePoint` doesn't compile, and shouldn't.
- **`steady_clock` vs `system_clock`.** `steady_clock` only ever moves forward, at a constant
  rate. `system_clock` is wall time and can jump — NTP correction, DST, the user changing the
  date. A countdown on `system_clock` can go backwards. The header already picks `steady_clock`;
  know why.

### The flaw this design exists to avoid

The naive timer decrements a counter every tick:

```cpp
remaining -= elapsed_since_last_tick;   // don't
```

Two failures. **Drift:** every tick rounds, and the errors accumulate — a few seconds an hour.
**Sleep:** close the laptop for two hours and the naive version loses two hours, because no
ticks happened.

The fix is to store an **absolute deadline** and derive `remaining` from it. When a phase ends,
the next deadline is measured *from the previous deadline*, not from `now`:

```
deadline_ = deadline_ + duration_of(next_phase);   // exact
deadline_ = now + duration_of(next_phase);         // drifts by the overshoot, every phase
```

`TimerTick.DoesNotDrift` and `TimerTick.RollsThroughSeveralPhasesInOneJump` are the two tests
that catch this. They're the point of the milestone.

## Your task

Implement every `TODO` in `src/pomodoro.cpp`. The header comments define the semantics
precisely; the tests are the authority where you think they disagree (tell me if they do —
that's a real finding, not a mistake on your part).

Suggested order, each turning a named group green:

1. `clamp_minutes`, `phase_after`, `duration_of` — pure, no state.
2. `Timer` constructor + observers.
3. `start` / `pause` / `toggle` / `reset`.
4. `tick` — the one with teeth.
5. `skip` / `set_config`.

### The invariant to hold in your head

At any moment exactly one field is authoritative:

- **running** → `deadline_` is truth, `remaining_` is a stale cache.
- **paused** → `remaining_` is truth, `deadline_` is meaningless.

`start` converts remaining → deadline. `pause` converts deadline → remaining. Every bug in this
class will be a place where you read the non-authoritative one.

## Acceptance

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

26 tests, 0 failures. That's the whole pass/fail signal. Currently **26/26 green**, warning-clean
under `-Wall -Wextra -Wpedantic`, and green under the `asan` preset (ASan + UBSan).

## Hints & research (pointers, not answers)

- `std::lround` — rounds half away from zero, returns `long`. Relevant to `clamp_minutes`.
- `std::clamp` (`<algorithm>`) — C++17, takes `(v, lo, hi)`.
- `std::chrono::duration_cast` — https://en.cppreference.com/w/cpp/chrono/duration/duration_cast
- `std::max` for the "never negative" requirement.
- A `switch` over a scoped enum with no `default:` gets you `-Wswitch` coverage warnings when
  you miss a case. That's a feature — don't add `default:`.
- `tick` rolling several phases wants a **loop**, not an `if`. Ask yourself what terminates it
  when `now` is a year past the deadline, and whether the loop body can ever fail to advance
  the deadline (an infinite loop is reachable here if a duration can be zero — is it?).
- For `set_config`: "did the current phase's length change" is a comparison between the old
  and new config, before you overwrite the member.

## Constraints

- C++20, no external dependencies in the core. `<chrono>`, `<algorithm>`, `<cmath>` only.
- Don't change `pomodoro.hpp` — if the interface is wrong, say so and I'll change it.
- Don't edit the tests to make them pass.
- Warnings are on (`-Wall -Wextra -Wpedantic`). Ship it warning-clean.

## Stretch goals (optional, after green)

- Make `phase_after` and `duration_of` `constexpr`. What has to change? Does `clamp_minutes`
  follow, given `std::lround` isn't `constexpr`?
- Mark the observers `[[nodiscard]]` and see what the compiler catches.
- Add a `static_assert` proving `duration_of` on a default config equals 25 minutes at compile
  time — only possible if the previous step worked.

## My working notes

<!-- scratchpad — yours -->
