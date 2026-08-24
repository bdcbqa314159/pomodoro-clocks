#include "config_io.hpp"
#include "pomodoro.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;
using namespace std::chrono_literals;

namespace {

// --- big digits -------------------------------------------------------------
// 3x5 blocks, index 0-9, plus the colon at [10] (1 column wide).
constexpr int kGlyphRows = 5;
const std::vector<std::vector<std::string>> kGlyphs = {
    {"███", "█ █", "█ █", "█ █", "███"},  // 0
    {"  █", "  █", "  █", "  █", "  █"},  // 1
    {"███", "  █", "███", "█  ", "███"},  // 2
    {"███", "  █", "███", "  █", "███"},  // 3
    {"█ █", "█ █", "███", "  █", "  █"},  // 4
    {"███", "█  ", "███", "  █", "███"},  // 5
    {"███", "█  ", "███", "█ █", "███"},  // 6
    {"███", "  █", "  █", "  █", "  █"},  // 7
    {"███", "█ █", "███", "█ █", "███"},  // 8
    {"███", "█ █", "███", "  █", "███"},  // 9
    {" ", "█", " ", "█", " "},            // 10 = colon
};

/// Render mm:ss as five rows of block glyphs.
std::vector<std::string> big_time(int total_seconds) {
  const int mm = total_seconds / 60;
  const int ss = total_seconds % 60;
  const std::vector<int> cells = {mm / 10, mm % 10, 10, ss / 10, ss % 10};

  std::vector<std::string> rows(kGlyphRows);
  for (int r = 0; r < kGlyphRows; ++r) {
    for (size_t i = 0; i < cells.size(); ++i) {
      if (i > 0) {
        rows[r] += " ";
      }
      rows[r] += kGlyphs[static_cast<size_t>(cells[i])][static_cast<size_t>(r)];
    }
  }
  return rows;
}

const char* label_of(pomo::Phase p) {
  switch (p) {
    case pomo::Phase::Focus: return "F O C U S";
    case pomo::Phase::ShortBreak: return "S H O R T   B R E A K";
    case pomo::Phase::LongBreak: return "L O N G   B R E A K";
  }
  return "";
}

// --- alerts -----------------------------------------------------------------
// ponytail: shell out. A UserNotifications binding needs a code-signed .app
// bundle to deliver the same two lines, and CoreAudio needs a run loop.
// Strings are compile-time constants, so there is nothing to inject here.
void alert(const char* title, const char* body) {
#if defined(__APPLE__)
  const std::string cmd = std::string("osascript -e 'display notification \"") + body +
                          "\" with title \"" + title + "\" sound name \"Glass\"' " +
                          ">/dev/null 2>&1 &";  // & — never block the render loop
  (void)std::system(cmd.c_str());
#else
  (void)title;
  (void)body;
  std::fputs("\a", stdout);  // ponytail: terminal bell. Wire libnotify/PowerShell if you port.
  std::fflush(stdout);
#endif
}

}  // namespace

int main() {
  pomo::Timer timer{pomo::load_config()};
  auto screen = ScreenInteractive::TerminalOutput();

  bool settings_open = false;
  bool saved_ok = true;
  int field = 0;  // index into kFields

  // Empty path == no home directory anywhere; say so rather than printing "".
  const auto cfg_path = pomo::config_path();
  const std::string config_location =
      cfg_path.empty() ? std::string("no home directory - settings are not persisted")
                       : cfg_path.string();
  const std::string save_error = cfg_path.empty()
                                     ? "settings not saved - no home directory"
                                     : "settings not saved - could not write " + cfg_path.string();

  struct Field {
    const char* label;
    int pomo::Config::*member;
    int max;
    const char* unit;
  };
  // ponytail: ±/arrow stepping, not text entry — no cursor, no parse, no partial
  // state, and clamp_minutes already exists to bound it.
  static const Field kFields[] = {
      {"Focus", &pomo::Config::focus_min, pomo::kMaxMinutes, "min"},
      {"Short break", &pomo::Config::short_min, pomo::kMaxMinutes, "min"},
      {"Long break", &pomo::Config::long_min, pomo::kMaxMinutes, "min"},
      {"Rounds", &pomo::Config::rounds, pomo::kMaxRounds, ""},
  };
  constexpr int kFieldCount = static_cast<int>(std::size(kFields));

  auto adjust = [&](int delta) {
    pomo::Config cfg = timer.config();
    const Field& f = kFields[field];
    cfg.*(f.member) = pomo::clamp_minutes(cfg.*(f.member) + delta, f.max);
    timer.set_config(cfg);  // retimes (and stops) only if the current phase changed length
  };

  auto renderer = Renderer([&] {
    const int ended = timer.tick(pomo::Clock::now());
    if (ended > 0) {
      // Only announce the phase we landed on — a two-hour sleep must not fire
      // seven notifications on wake.
      if (timer.phase() == pomo::Phase::Focus) {
        alert("Break over", "Back to focus.");
      } else {
        alert("Focus done", "Time for a break.");
      }
    }

    const auto total = pomo::duration_of(timer.config(), timer.phase());
    const auto left = timer.remaining();
    const double ratio =
        total.count() > 0 ? static_cast<double>(left.count()) / static_cast<double>(total.count())
                          : 0.0;

    const Color accent = timer.phase() == pomo::Phase::Focus ? Color::Salmon1 : Color::Aquamarine1;

    // Round up, so the display reads 25:00 the instant a phase starts and only
    // shows 0:00 when the phase is genuinely over.
    const int secs = static_cast<int>((left.count() + 999) / 1000);

    Elements digits;
    for (const auto& row : big_time(secs)) {
      digits.push_back(text(row) | color(accent));
    }

    std::string dots;
    const int in_set = timer.config().rounds > 0 ? timer.completed() % timer.config().rounds : 0;
    const bool full = timer.completed() > 0 && in_set == 0;
    for (int i = 0; i < timer.config().rounds; ++i) {
      dots += (i < in_set || full) ? "● " : "○ ";
    }

    if (settings_open) {
      Elements rows;
      for (int i = 0; i < kFieldCount; ++i) {
        const Field& f = kFields[i];
        const bool on = (i == field);
        auto row = hbox({
            text(on ? " > " : "   "),
            text(f.label) | flex,
            text(std::to_string(timer.config().*(f.member))) | align_right |
                size(WIDTH, EQUAL, 4),
            text(std::string(" ") + f.unit) | size(WIDTH, EQUAL, 4),
        });
        rows.push_back(on ? (row | bold | color(accent)) : (row | dim));
      }
      return vbox({
                 text("S E T T I N G S") | bold | color(accent) | hcenter,
                 text("") | size(HEIGHT, EQUAL, 1),
                 vbox(std::move(rows)),
                 text("") | size(HEIGHT, EQUAL, 1),
                 text("up/down select   left/right adjust   esc close") | dim | hcenter,
                 text(config_location) | dim | hcenter,
             }) |
             border | size(WIDTH, EQUAL, 52) | center;  // wide enough for the hint line
    }

    Element save_warning = saved_ok ? filler() | size(HEIGHT, EQUAL, 0)
                                    : text(save_error) | color(Color::Red) | hcenter;

    return vbox({
               text(label_of(timer.phase())) | bold | color(accent) | hcenter,
               text("") | size(HEIGHT, EQUAL, 1),
               vbox(std::move(digits)) | hcenter,
               text("") | size(HEIGHT, EQUAL, 1),
               gauge(1.0f - static_cast<float>(ratio)) | color(accent) |
                   size(WIDTH, EQUAL, 34) | hcenter,
               text("") | size(HEIGHT, EQUAL, 1),
               text(dots) | color(accent) | hcenter,
               text(timer.running() ? "running" : "paused") | dim | hcenter,
               text("") | size(HEIGHT, EQUAL, 1),
               text("space pause   r reset   s skip   c settings   q quit") | dim | hcenter,
               std::move(save_warning),
           }) |
           border | center;
  });

  auto app = CatchEvent(renderer, [&](const Event& e) {
    const auto now = pomo::Clock::now();

    // Settings owns every key while it is open, so nothing here can also start
    // the timer or quit the app by accident.
    if (settings_open) {
      if (e == Event::ArrowUp) {
        field = (field + kFieldCount - 1) % kFieldCount;
      } else if (e == Event::ArrowDown) {
        field = (field + 1) % kFieldCount;
      } else if (e == Event::ArrowLeft) {
        adjust(-1);
      } else if (e == Event::ArrowRight) {
        adjust(+1);
      } else if (e == Event::Escape || e == Event::Return || e == Event::Character('c')) {
        settings_open = false;
        // Once on close, not once per arrow press. If the write fails (read-only
        // $HOME) the app carries on with the settings applied in memory.
        saved_ok = pomo::save_config(timer.config());
      }
      return true;
    }

    if (e == Event::Character('c')) {
      settings_open = true;
      return true;
    }
    if (e == Event::Character(' ')) {
      timer.toggle(now);
      return true;
    }
    if (e == Event::Character('r')) {
      timer.reset();
      return true;
    }
    if (e == Event::Character('s')) {
      timer.skip(now);
      return true;
    }
    if (e == Event::Character('q') || e == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // The core is passive — nothing advances unless something asks it to. This
  // thread is the only reason the display moves on its own.
  std::atomic<bool> done{false};
  std::thread ticker([&] {
    while (!done) {
      std::this_thread::sleep_for(200ms);
      screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(app);
  done = true;
  ticker.join();
  return 0;
}
