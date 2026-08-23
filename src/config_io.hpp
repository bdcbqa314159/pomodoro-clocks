#pragma once

#include "pomodoro.hpp"

#include <filesystem>
#include <iosfwd>

// Reading/writing the config file. Kept out of pomodoro.hpp so the timer core
// stays free of I/O — the parsing half is pure and tested against stringstreams.

namespace pomo {

/// $XDG_CONFIG_HOME/pomodoro/config, else $HOME/.config/pomodoro/config,
/// else ./pomodoro.conf when neither is set.
std::filesystem::path config_path();

/// Never throws and never fails: unknown keys, junk values, missing lines and
/// out-of-range numbers all fall back to the default for that field.
Config parse_config(std::istream& in);

void write_config(std::ostream& out, const Config& cfg);

/// parse_config() on config_path(). Defaults if the file is absent/unreadable.
Config load_config();

/// Best-effort. Returns false if the file could not be written.
bool save_config(const Config& cfg);

}  // namespace pomo
