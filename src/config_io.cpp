#include "config_io.hpp"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace pomo {
namespace {

std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t\r");
  if (first == std::string_view::npos) {
    return {};
  }
  return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

/// getenv, but an empty string counts as unset — an exported-but-blank HOME
/// would otherwise resolve to "/.config/pomodoro/config".
const char *env(const char *name) {
  const char *v = std::getenv(name);
  return (v != nullptr && *v != '\0') ? v : nullptr;
}

} // namespace

std::filesystem::path config_path() {
  namespace fs = std::filesystem;
  if (const char *xdg = env("XDG_CONFIG_HOME"); xdg != nullptr) {
    return fs::path(xdg) / "pomodoro" / "config";
  }
  if (const char *home = env("HOME"); home != nullptr) {
    return fs::path(home) / ".config" / "pomodoro" / "config";
  }
  if (const char *appdata = env("APPDATA"); appdata != nullptr) { // Windows
    return fs::path(appdata) / "pomodoro" / "config";
  }
  return {}; // no home directory at all: run on defaults, persist nothing
}

Config parse_config(std::istream &in) {
  Config cfg; // anything the file does not say keeps the default
  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue; // blank line, comment, garbage — skip it
    }
    const std::string_view key = trim(std::string_view(line).substr(0, eq));
    const std::string_view val = trim(std::string_view(line).substr(eq + 1));

    int n = 0;
    const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), n);
    if (ec != std::errc{} || ptr != val.data() + val.size()) {
      continue; // "focus=abc" or "focus=25x": keep the default
    }

    // Clamp on the way in. A hand-edited "focus=0" must not produce a zero-length
    // phase — Timer::tick would have to defend against it forever otherwise.
    if (key == "focus") {
      cfg.focus_min = clamp_minutes(n, kMaxMinutes);
    } else if (key == "short") {
      cfg.short_min = clamp_minutes(n, kMaxMinutes);
    } else if (key == "long") {
      cfg.long_min = clamp_minutes(n, kMaxMinutes);
    } else if (key == "rounds") {
      cfg.rounds = clamp_minutes(n, kMaxRounds);
    }
  }
  return cfg;
}

void write_config(std::ostream &out, const Config &cfg) {
  out << "focus=" << cfg.focus_min << "\n"
      << "short=" << cfg.short_min << "\n"
      << "long=" << cfg.long_min << "\n"
      << "rounds=" << cfg.rounds << "\n";
}

Config load_config() {
  const auto path = config_path();
  if (path.empty()) {
    return Config{};
  }
  std::ifstream in(path);
  if (!in) {
    return Config{};
  }
  return parse_config(in);
}

bool save_config(const Config &cfg) {
  const auto path = config_path();
  if (path.empty()) {
    return false; // nowhere to put it; the caller surfaces this
  }
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec); // ec: never throws
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    return false;
  }
  write_config(out, cfg);
  return out.good();
}

} // namespace pomo
