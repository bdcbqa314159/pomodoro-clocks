#include "pomodoro.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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
  pomo::Timer timer;
  auto screen = ScreenInteractive::TerminalOutput();

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
               text("space start/pause   r reset   s skip   q quit") | dim | hcenter,
           }) |
           border | center;
  });

  auto app = CatchEvent(renderer, [&](const Event& e) {
    const auto now = pomo::Clock::now();
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
