#include "config_io.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

using pomo::Config;

namespace {

Config parse(const std::string &text) {
  std::istringstream in(text);
  return pomo::parse_config(in);
}

TEST(ParseConfig, ReadsEveryField) {
  const Config c = parse("focus=50\nshort=10\nlong=30\nrounds=3\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.short_min, 10);
  EXPECT_EQ(c.long_min, 30);
  EXPECT_EQ(c.rounds, 3);
}

TEST(ParseConfig, EmptyInputGivesDefaults) {
  const Config c = parse("");
  EXPECT_EQ(c.focus_min, 25);
  EXPECT_EQ(c.short_min, 5);
  EXPECT_EQ(c.long_min, 15);
  EXPECT_EQ(c.rounds, 4);
}

TEST(ParseConfig, MissingFieldsKeepTheirDefaults) {
  const Config c = parse("focus=50\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.short_min, 5); // untouched
}

TEST(ParseConfig, ToleratesWhitespace) {
  const Config c = parse("  focus = 50  \n\tshort=10\t\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.short_min, 10);
}

TEST(ParseConfig, IgnoresJunkLines) {
  const Config c = parse("this is not a config\n\n# comment\nfocus=50\n=\nlong=\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.long_min, 15); // "long=" is not a number: default survives
}

TEST(ParseConfig, RejectsNonNumericAndTrailingGarbage) {
  EXPECT_EQ(parse("focus=abc\n").focus_min, 25);
  EXPECT_EQ(parse("focus=25x\n").focus_min, 25);
  EXPECT_EQ(parse("focus=2.5\n").focus_min, 25); // no partial reads
}

TEST(ParseConfig, ClampsHandEditedValues) {
  // The whole point: a hand-edited 0 must not reach Timer as a zero-length phase.
  EXPECT_EQ(parse("focus=0\n").focus_min, 1);
  EXPECT_EQ(parse("focus=-5\n").focus_min, 1);
  EXPECT_EQ(parse("focus=99999\n").focus_min, pomo::kMaxMinutes);
  EXPECT_EQ(parse("rounds=0\n").rounds, 1);
  EXPECT_EQ(parse("rounds=500\n").rounds, pomo::kMaxRounds);
}

TEST(ParseConfig, UnknownKeysAreIgnored) {
  const Config c = parse("colour=blue\nfocus=50\n");
  EXPECT_EQ(c.focus_min, 50);
}

TEST(WriteConfig, RoundTrips) {
  const Config original{50, 10, 30, 3};
  std::ostringstream out;
  pomo::write_config(out, original);

  const Config back = parse(out.str());
  EXPECT_EQ(back.focus_min, original.focus_min);
  EXPECT_EQ(back.short_min, original.short_min);
  EXPECT_EQ(back.long_min, original.long_min);
  EXPECT_EQ(back.rounds, original.rounds);
}

#ifndef _WIN32 // setenv/unsetenv are POSIX; these three are skipped on Windows

// Restores whatever was there before, so test order stays irrelevant.
class ScopedEnv {
public:
  ScopedEnv(const char *name, const char *value) : name_(name) {
    if (const char *old = std::getenv(name); old != nullptr) {
      had_ = true;
      old_ = old;
    }
    set(value);
  }
  ~ScopedEnv() { set(had_ ? old_.c_str() : nullptr); }
  ScopedEnv(const ScopedEnv &) = delete;
  ScopedEnv &operator=(const ScopedEnv &) = delete;

private:
  void set(const char *v) const {
    if (v != nullptr) {
      ::setenv(name_, v, 1);
    } else {
      ::unsetenv(name_);
    }
  }
  const char *name_;
  bool had_ = false;
  std::string old_;
};

TEST(ConfigPath, PrefersXdgConfigHome) {
  ScopedEnv xdg("XDG_CONFIG_HOME", "/tmp/xdg");
  ScopedEnv home("HOME", "/tmp/home");
  EXPECT_EQ(pomo::config_path(), std::filesystem::path("/tmp/xdg/pomodoro/config"));
}

TEST(ConfigPath, FallsBackToDotConfigUnderHome) {
  ScopedEnv xdg("XDG_CONFIG_HOME", nullptr);
  ScopedEnv home("HOME", "/tmp/home");
  EXPECT_EQ(pomo::config_path(), std::filesystem::path("/tmp/home/.config/pomodoro/config"));
}

TEST(ConfigPath, TreatsBlankEnvVarsAsUnset) {
  ScopedEnv xdg("XDG_CONFIG_HOME", "");
  ScopedEnv home("HOME", "/tmp/home");
  EXPECT_EQ(pomo::config_path(), std::filesystem::path("/tmp/home/.config/pomodoro/config"));
}

TEST(ConfigPath, IsEmptyWithNoHomeAtAll) {
  // The old behaviour dropped pomodoro.conf into the working directory, which
  // gave you a different config per directory you launched from.
  ScopedEnv xdg("XDG_CONFIG_HOME", nullptr);
  ScopedEnv home("HOME", nullptr);
  ScopedEnv appdata("APPDATA", nullptr);
  EXPECT_TRUE(pomo::config_path().empty());
}

TEST(SaveConfig, FailsCleanlyWithNowhereToWrite) {
  ScopedEnv xdg("XDG_CONFIG_HOME", nullptr);
  ScopedEnv home("HOME", nullptr);
  ScopedEnv appdata("APPDATA", nullptr);
  EXPECT_FALSE(pomo::save_config(Config{}));
  EXPECT_EQ(pomo::load_config().focus_min, 25); // still usable, just not persisted
}

TEST(SaveConfig, RoundTripsThroughARealFile) {
  const auto dir = std::filesystem::temp_directory_path() / "pomodoro_test_cfg";
  std::filesystem::remove_all(dir);
  ScopedEnv xdg("XDG_CONFIG_HOME", dir.string().c_str());

  EXPECT_TRUE(pomo::save_config(Config{40, 8, 20, 2}));
  EXPECT_TRUE(std::filesystem::exists(dir / "pomodoro" / "config"));

  const Config back = pomo::load_config();
  EXPECT_EQ(back.focus_min, 40);
  EXPECT_EQ(back.short_min, 8);
  EXPECT_EQ(back.long_min, 20);
  EXPECT_EQ(back.rounds, 2);

  std::filesystem::remove_all(dir);
}

#endif // _WIN32

} // namespace
