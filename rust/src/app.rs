//! The UI. Compiled unchanged for the native window and for the browser canvas.

use eframe::egui::{self, Color32, FontId, Pos2, Sense, Stroke, Vec2};

use crate::alert;
use crate::timer::{clamp_minutes, Config, Phase, Timer, MAX_MINUTES, MAX_ROUNDS};

const FOCUS_COLOR: Color32 = Color32::from_rgb(255, 107, 91);
const BREAK_COLOR: Color32 = Color32::from_rgb(69, 201, 165);
const TRACK_COLOR: Color32 = Color32::from_rgb(48, 54, 68);

pub struct PomodoroApp {
    timer: Timer,
    settings_open: bool,
    /// Notification permission is asked on the first Start — a user gesture.
    /// Browsers penalise permission prompts fired on page load.
    asked_permission: bool,
}

impl PomodoroApp {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let cfg = cc
            .storage
            .and_then(|s| eframe::get_value::<Config>(s, eframe::APP_KEY))
            .unwrap_or_default();

        let mut visuals = egui::Visuals::dark();
        visuals.panel_fill = Color32::from_rgb(16, 19, 26);
        cc.egui_ctx.set_visuals(visuals);

        Self {
            timer: Timer::new(cfg),
            settings_open: false,
            asked_permission: false,
        }
    }

    fn accent(&self) -> Color32 {
        if self.timer.phase() == Phase::Focus {
            FOCUS_COLOR
        } else {
            BREAK_COLOR
        }
    }

    fn toggle(&mut self, now: f64) {
        if !self.timer.running() && !self.asked_permission {
            alert::request_permission();
            self.asked_permission = true;
        }
        self.timer.toggle(now);
    }

    /// The ring: a track circle plus an arc spanning the elapsed fraction, with
    /// mm:ss painted in the middle.
    fn draw_ring(&self, ui: &mut egui::Ui, diameter: f32) {
        let (rect, _) = ui.allocate_exact_size(Vec2::splat(diameter), Sense::hover());
        let painter = ui.painter();
        let center = rect.center();
        let radius = diameter * 0.42;
        let accent = self.accent();

        painter.circle_stroke(center, radius, Stroke::new(3.0, TRACK_COLOR));

        let progress = self.timer.progress();
        if progress > 0.0 {
            // 96 segments is smooth at any size we render and costs nothing.
            let steps = (96.0 * progress).ceil().max(2.0) as usize;
            let sweep = std::f32::consts::TAU * progress;
            let points: Vec<Pos2> = (0..=steps)
                .map(|i| {
                    let t = sweep * (i as f32 / steps as f32) - std::f32::consts::FRAC_PI_2;
                    center + Vec2::new(t.cos(), t.sin()) * radius
                })
                .collect();
            painter.add(egui::Shape::line(points, Stroke::new(7.0, accent)));
        }

        let secs = self.timer.remaining().ceil().max(0.0) as u64;
        painter.text(
            center,
            egui::Align2::CENTER_CENTER,
            format!("{}:{:02}", secs / 60, secs % 60),
            FontId::proportional(diameter * 0.19),
            Color32::from_rgb(232, 236, 244),
        );
    }

    fn draw_settings(&mut self, ui: &mut egui::Ui) {
        let mut cfg = self.timer.config();
        let mut changed = false;

        ui.add_space(4.0);
        for (label, value, max) in [
            ("Focus", &mut cfg.focus_min, MAX_MINUTES),
            ("Short break", &mut cfg.short_min, MAX_MINUTES),
            ("Long break", &mut cfg.long_min, MAX_MINUTES),
        ] {
            changed |= ui
                .add(egui::Slider::new(value, 1..=max).text(label).suffix(" min"))
                .changed();
        }
        changed |= ui
            .add(egui::Slider::new(&mut cfg.rounds, 1..=MAX_ROUNDS).text("Rounds"))
            .changed();

        if changed {
            // Belt and braces: the sliders are already bounded, but everything
            // that reaches Config goes through the same clamp.
            cfg.focus_min = clamp_minutes(f64::from(cfg.focus_min), MAX_MINUTES);
            cfg.short_min = clamp_minutes(f64::from(cfg.short_min), MAX_MINUTES);
            cfg.long_min = clamp_minutes(f64::from(cfg.long_min), MAX_MINUTES);
            cfg.rounds = clamp_minutes(f64::from(cfg.rounds), MAX_ROUNDS);
            self.timer.set_config(cfg);
        }
    }
}

impl eframe::App for PomodoroApp {
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        eframe::set_value(storage, eframe::APP_KEY, &self.timer.config());
    }

    /// Runs before every `ui` pass, and also while the window is hidden — so the
    /// timer keeps its phase transitions and notifications even when nothing is
    /// being drawn.
    fn logic(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // egui's frame time is monotonic seconds since start, on native and web
        // alike — which is why the core takes f64 instead of Instant.
        let now = ctx.input(|i| i.time);

        if self.timer.tick(now) > 0 {
            // Announce only the phase we landed on: waking from a two-hour sleep
            // must not fire seven notifications.
            if self.timer.phase() == Phase::Focus {
                alert::alert("Break over", "Back to focus.");
            } else {
                alert::alert("Focus done", "Time for a break.");
            }
        }

        // The core is passive: without this the UI only repaints on input.
        ctx.request_repaint_after(std::time::Duration::from_millis(200));
    }

    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();
        let now = ctx.input(|i| i.time);

        let (space, r, s, c) = ctx.input(|i| {
            (
                i.key_pressed(egui::Key::Space),
                i.key_pressed(egui::Key::R),
                i.key_pressed(egui::Key::S),
                i.key_pressed(egui::Key::C),
            )
        });
        if space {
            self.toggle(now);
        }
        if r {
            self.timer.reset();
        }
        if s {
            self.timer.skip(now);
        }
        if c {
            self.settings_open = !self.settings_open;
        }

        let accent = self.accent();

        {
            ui.vertical_centered(|ui| {
                ui.add_space(14.0);
                ui.label(
                    egui::RichText::new(self.timer.phase().label())
                        .color(accent)
                        .size(15.0)
                        .strong(),
                );
                ui.add_space(10.0);

                let diameter = (ui.available_width().min(ui.available_height() + 120.0) * 0.72)
                    .clamp(160.0, 320.0);
                self.draw_ring(ui, diameter);

                ui.add_space(12.0);
                ui.horizontal(|ui| {
                    // Centre the button row inside the full-width layout.
                    let buttons = 3.0f32.mul_add(96.0, 16.0);
                    ui.add_space(((ui.available_width() - buttons) * 0.5).max(0.0));
                    if ui
                        .add_sized([96.0, 30.0], egui::Button::new(if self.timer.running() {
                            "Pause"
                        } else {
                            "Start"
                        }))
                        .clicked()
                    {
                        self.toggle(now);
                    }
                    if ui.add_sized([80.0, 30.0], egui::Button::new("Reset")).clicked() {
                        self.timer.reset();
                    }
                    if ui.add_sized([80.0, 30.0], egui::Button::new("Skip")).clicked() {
                        self.timer.skip(now);
                    }
                });

                ui.add_space(10.0);
                let rounds = self.timer.config().rounds.max(1);
                let in_set = self.timer.completed() % rounds;
                let full = self.timer.completed() > 0 && in_set == 0;
                let dots: String = (0..rounds)
                    .map(|i| if i < in_set || full { "● " } else { "○ " })
                    .collect();
                ui.label(egui::RichText::new(dots).color(accent).size(15.0));

                ui.add_space(6.0);
                ui.label(
                    egui::RichText::new("space  pause    r  reset    s  skip    c  settings")
                        .size(11.0)
                        .weak(),
                );

                ui.add_space(8.0);
                if ui
                    .selectable_label(self.settings_open, "Settings")
                    .clicked()
                {
                    self.settings_open = !self.settings_open;
                }
                if self.settings_open {
                    ui.add_space(4.0);
                    self.draw_settings(ui);
                }
            });
        }
    }
}
