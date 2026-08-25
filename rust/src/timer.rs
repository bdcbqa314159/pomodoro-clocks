//! Pure timer core. No clock reads, no I/O.
//!
//! Time is `f64` seconds on *some* monotonic clock, supplied by the caller.
//! Not `std::time::Instant`: that panics on `wasm32-unknown-unknown`, and the
//! same core has to run in the browser. Native passes `Instant::elapsed`, web
//! passes egui's frame time; neither leaks in here.

use serde::{Deserialize, Serialize};

pub const MAX_MINUTES: u32 = 180;
pub const MAX_ROUNDS: u32 = 12;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Phase {
    Focus,
    ShortBreak,
    LongBreak,
}

impl Phase {
    pub fn label(self) -> &'static str {
        match self {
            Self::Focus => "FOCUS",
            Self::ShortBreak => "SHORT BREAK",
            Self::LongBreak => "LONG BREAK",
        }
    }

    pub fn is_break(self) -> bool {
        self != Self::Focus
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Config {
    pub focus_min: u32,
    pub short_min: u32,
    pub long_min: u32,
    pub rounds: u32,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            focus_min: 25,
            short_min: 5,
            long_min: 15,
            rounds: 4,
        }
    }
}

impl Config {
    pub fn minutes(&self, phase: Phase) -> u32 {
        match phase {
            Phase::Focus => self.focus_min,
            Phase::ShortBreak => self.short_min,
            Phase::LongBreak => self.long_min,
        }
    }

    pub fn seconds(&self, phase: Phase) -> f64 {
        f64::from(self.minutes(phase)) * 60.0
    }
}

/// Round to nearest, then clamp into `1..=max`.
pub fn clamp_minutes(value: f64, max: u32) -> u32 {
    let rounded = value.round();
    if rounded.is_nan() {
        return 1;
    }
    rounded.clamp(1.0, f64::from(max)) as u32
}

/// After a focus block: a long break when `completed` is a multiple of `rounds`,
/// otherwise a short one. After any break: focus. `completed` includes the block
/// that just ended.
pub fn phase_after(current: Phase, completed: u32, rounds: u32) -> Phase {
    if current != Phase::Focus {
        return Phase::Focus;
    }
    if rounds > 0 && completed.is_multiple_of(rounds) {
        Phase::LongBreak
    } else {
        Phase::ShortBreak
    }
}

#[derive(Clone, Debug)]
pub struct Timer {
    cfg: Config,
    phase: Phase,
    completed: u32,
    running: bool,
    /// Authoritative while paused.
    remaining: f64,
    /// Authoritative while running.
    deadline: f64,
}

impl Timer {
    pub fn new(cfg: Config) -> Self {
        Self {
            cfg,
            phase: Phase::Focus,
            completed: 0,
            running: false,
            remaining: cfg.seconds(Phase::Focus),
            deadline: 0.0,
        }
    }

    pub fn phase(&self) -> Phase {
        self.phase
    }
    pub fn completed(&self) -> u32 {
        self.completed
    }
    pub fn running(&self) -> bool {
        self.running
    }
    pub fn remaining(&self) -> f64 {
        self.remaining
    }
    pub fn config(&self) -> Config {
        self.cfg
    }

    /// Fraction of the current phase already spent, in `0.0..=1.0`.
    pub fn progress(&self) -> f32 {
        let total = self.cfg.seconds(self.phase);
        if total <= 0.0 {
            return 0.0;
        }
        (1.0 - self.remaining / total).clamp(0.0, 1.0) as f32
    }

    pub fn start(&mut self, now: f64) {
        if self.running {
            return; // idempotent: must not push the deadline out
        }
        self.deadline = now + self.remaining;
        self.running = true;
    }

    pub fn pause(&mut self, now: f64) {
        if !self.running {
            return;
        }
        self.remaining = (self.deadline - now).max(0.0);
        self.running = false;
    }

    pub fn toggle(&mut self, now: f64) {
        if self.running {
            self.pause(now);
        } else {
            self.start(now);
        }
    }

    /// Back to the top of the current phase, stopped.
    pub fn reset(&mut self) {
        self.running = false;
        self.remaining = self.cfg.seconds(self.phase);
    }

    /// End the current phase early. Preserves `running`.
    pub fn skip(&mut self, now: f64) {
        if self.phase == Phase::Focus {
            self.completed += 1;
        }
        self.phase = phase_after(self.phase, self.completed, self.cfg.rounds);
        self.remaining = self.cfg.seconds(self.phase);
        if self.running {
            self.deadline = now + self.remaining;
        }
    }

    /// Advance to `now`, rolling through every phase whose deadline has passed.
    /// Returns how many phases ended.
    pub fn tick(&mut self, now: f64) -> u32 {
        if !self.running {
            return 0;
        }

        let mut transitions = 0;
        while now >= self.deadline {
            if self.phase == Phase::Focus {
                self.completed += 1;
            }
            self.phase = phase_after(self.phase, self.completed, self.cfg.rounds);
            transitions += 1;

            let next = self.cfg.seconds(self.phase);
            // A zero-length phase never moves the deadline past `now`: that is an
            // infinite loop, not a fast timer.
            if next <= 0.0 {
                self.running = false;
                self.remaining = 0.0;
                return transitions;
            }
            // From the previous deadline, never from `now` — otherwise the
            // overshoot is silently forgiven once per phase and the clock drifts.
            self.deadline += next;
        }

        self.remaining = (self.deadline - now).max(0.0);
        transitions
    }

    /// Retimes (and stops) the current phase only if its length changed.
    pub fn set_config(&mut self, cfg: Config) {
        let before = self.cfg.seconds(self.phase);
        self.cfg = cfg;
        if (self.cfg.seconds(self.phase) - before).abs() > f64::EPSILON {
            self.reset();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const T0: f64 = 0.0;

    fn mins(m: f64) -> f64 {
        m * 60.0
    }

    fn close(a: f64, b: f64) -> bool {
        (a - b).abs() < 1e-9
    }

    #[test]
    fn clamp_minutes_clamps_into_range() {
        assert_eq!(clamp_minutes(25.0, 180), 25);
        assert_eq!(clamp_minutes(0.0, 180), 1);
        assert_eq!(clamp_minutes(-5.0, 180), 1);
        assert_eq!(clamp_minutes(999.0, 180), 180);
    }

    #[test]
    fn clamp_minutes_rounds_to_nearest() {
        assert_eq!(clamp_minutes(25.4, 180), 25);
        assert_eq!(clamp_minutes(25.6, 180), 26);
        assert_eq!(clamp_minutes(2.5, 180), 3);
    }

    #[test]
    fn phase_after_mid_set_and_on_boundary() {
        assert_eq!(phase_after(Phase::Focus, 1, 4), Phase::ShortBreak);
        assert_eq!(phase_after(Phase::Focus, 4, 4), Phase::LongBreak);
        assert_eq!(phase_after(Phase::Focus, 8, 4), Phase::LongBreak);
        assert_eq!(phase_after(Phase::Focus, 2, 2), Phase::LongBreak);
        assert_eq!(phase_after(Phase::ShortBreak, 1, 4), Phase::Focus);
        assert_eq!(phase_after(Phase::LongBreak, 4, 4), Phase::Focus);
    }

    #[test]
    fn starts_stopped_at_top_of_focus() {
        let t = Timer::new(Config::default());
        assert_eq!(t.phase(), Phase::Focus);
        assert_eq!(t.completed(), 0);
        assert!(!t.running());
        assert!(close(t.remaining(), mins(25.0)));
    }

    #[test]
    fn start_then_tick_counts_down() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        assert!(close(t.remaining(), mins(25.0))); // start alone consumes nothing
        assert_eq!(t.tick(mins(10.0)), 0);
        assert!(close(t.remaining(), mins(15.0)));
    }

    #[test]
    fn start_is_idempotent() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.start(mins(5.0));
        t.tick(mins(10.0));
        assert!(close(t.remaining(), mins(15.0)));
    }

    #[test]
    fn pause_freezes_the_countdown() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.pause(mins(10.0));
        assert!(!t.running());
        assert_eq!(t.tick(mins(90.0)), 0);
        assert!(close(t.remaining(), mins(15.0)));

        t.start(mins(90.0));
        t.tick(mins(95.0));
        assert!(close(t.remaining(), mins(10.0)));
    }

    #[test]
    fn reset_returns_to_top_of_phase() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.tick(mins(10.0));
        t.reset();
        assert!(!t.running());
        assert!(close(t.remaining(), mins(25.0)));
        assert_eq!(t.completed(), 0);
    }

    #[test]
    fn remaining_never_goes_negative() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.pause(mins(999.0));
        assert!(t.remaining() >= 0.0);
    }

    #[test]
    fn focus_ends_exactly_on_the_deadline() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        assert_eq!(t.tick(mins(25.0)), 1);
        assert_eq!(t.phase(), Phase::ShortBreak);
        assert_eq!(t.completed(), 1);
        assert!(close(t.remaining(), mins(5.0)));
        assert!(t.running()); // auto-continues
    }

    #[test]
    fn does_not_drift() {
        // Overshoot by 0.5s: the break is 0.5s short, not a full 5 minutes.
        let mut t = Timer::new(Config::default());
        t.start(T0);
        assert_eq!(t.tick(mins(25.0) + 0.5), 1);
        assert!(close(t.remaining(), mins(5.0) - 0.5));
    }

    #[test]
    fn rolls_through_several_phases_in_one_jump() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        assert_eq!(t.tick(mins(31.0)), 2);
        assert_eq!(t.phase(), Phase::Focus);
        assert_eq!(t.completed(), 1);
        assert!(close(t.remaining(), mins(24.0)));
    }

    #[test]
    fn reaches_the_long_break_after_four_focus_blocks() {
        // 25,30,55,60,85,90,115 -> 7 transitions, 4 focus blocks.
        let mut t = Timer::new(Config::default());
        t.start(T0);
        assert_eq!(t.tick(mins(115.0)), 7);
        assert_eq!(t.phase(), Phase::LongBreak);
        assert_eq!(t.completed(), 4);
        assert!(close(t.remaining(), mins(15.0)));
    }

    #[test]
    fn stopped_timer_never_advances() {
        let mut t = Timer::new(Config::default());
        assert_eq!(t.tick(mins(500.0)), 0);
        assert_eq!(t.phase(), Phase::Focus);
    }

    #[test]
    fn skip_ends_focus_early_and_keeps_running() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.skip(mins(3.0));
        assert_eq!(t.phase(), Phase::ShortBreak);
        assert_eq!(t.completed(), 1);
        assert!(t.running());
        t.tick(mins(5.0));
        assert!(close(t.remaining(), mins(3.0)));
    }

    #[test]
    fn skip_on_a_stopped_timer_leaves_it_stopped() {
        let mut t = Timer::new(Config::default());
        t.skip(T0);
        assert_eq!(t.phase(), Phase::ShortBreak);
        assert!(!t.running());
    }

    #[test]
    fn skipping_a_break_does_not_credit_a_focus_block() {
        let mut t = Timer::new(Config::default());
        t.skip(T0);
        t.skip(T0);
        assert_eq!(t.phase(), Phase::Focus);
        assert_eq!(t.completed(), 1);
    }

    #[test]
    fn set_config_retimes_the_current_phase_when_it_changed() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.tick(mins(10.0));
        t.set_config(Config {
            focus_min: 50,
            ..Config::default()
        });
        assert!(close(t.remaining(), mins(50.0)));
        assert!(!t.running());
    }

    #[test]
    fn set_config_leaves_the_countdown_alone_when_another_phase_changed() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.tick(mins(10.0));
        t.set_config(Config {
            short_min: 10,
            ..Config::default()
        });
        assert!(close(t.remaining(), mins(15.0)));
        assert!(t.running());

        t.tick(mins(25.0));
        assert_eq!(t.phase(), Phase::ShortBreak);
        assert!(close(t.remaining(), mins(10.0)));
    }

    #[test]
    fn rounds_change_is_not_a_retime() {
        let mut t = Timer::new(Config::default());
        t.start(T0);
        t.tick(mins(10.0));
        t.set_config(Config {
            rounds: 2,
            ..Config::default()
        });
        assert!(t.running());
        assert!(close(t.remaining(), mins(15.0)));
    }
}
