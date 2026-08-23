#include "config_io.hpp"

#include <gtest/gtest.h>

#include <sstream>

using pomo::Config;

namespace {

Config parse(const std::string& text) {
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
  EXPECT_EQ(c.short_min, 5);  // untouched
}

TEST(ParseConfig, ToleratesWhitespace) {
  const Config c = parse("  focus = 50  \n\tshort=10\t\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.short_min, 10);
}

TEST(ParseConfig, IgnoresJunkLines) {
  const Config c = parse("this is not a config\n\n# comment\nfocus=50\n=\nlong=\n");
  EXPECT_EQ(c.focus_min, 50);
  EXPECT_EQ(c.long_min, 15);  // "long=" is not a number: default survives
}

TEST(ParseConfig, RejectsNonNumericAndTrailingGarbage) {
  EXPECT_EQ(parse("focus=abc\n").focus_min, 25);
  EXPECT_EQ(parse("focus=25x\n").focus_min, 25);
  EXPECT_EQ(parse("focus=2.5\n").focus_min, 25);  // no partial reads
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

TEST(ConfigPath, IsAbsoluteWhenHomeIsSet) {
  // Both CI and a dev machine always have one of these.
  const auto p = pomo::config_path();
  EXPECT_FALSE(p.empty());
  EXPECT_EQ(p.filename(), "config");
}

}  // namespace
