# pomodoro

A pomodoro timer, as a terminal app. C++20.

`prototype/index.html` is the working reference implementation — open it in a browser. The C++
port exists to rebuild it as a native TUI (and to learn C++ doing it).

## Run

```sh
cmake --preset release && cmake --build --preset release
./build/release/pomodoro
```

| key | |
|---|---|
| `space` | start / pause |
| `r` | reset the current phase |
| `s` | skip to the next phase |
| `c` | settings (arrows to select/adjust, `esc` to close) |
| `q` / `esc` | quit |

Defaults are 25 / 5 / 15 with a long break every 4. Changing them in the settings screen writes
`$XDG_CONFIG_HOME/pomodoro/config` (or `~/.config/pomodoro/config`) on close:

```
focus=25
short=5
long=15
rounds=4
```

Hand-editing it is fine — values are clamped and anything unparseable falls back to the default.

## Build & test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, `asan` (non-Windows). First configure takes ~2 min — it clones
FTXUI and GoogleTest.

## Layout

```
src/pomodoro.hpp     timer core — pure logic, no I/O, no clock reads
src/pomodoro.cpp     implementation
src/config_io.*      config file read/write — parsing half is pure and tested
src/main.cpp         FTXUI terminal app — the only place that reads a clock
tests/               GoogleTest, fake time
docs/                milestone notes
prototype/           the HTML original
```

## Dev tooling

Pinned locally, never installed globally:

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements-dev.txt
.venv/bin/clang-format -i $(git ls-files '*.hpp' '*.cpp')
```

`compile_commands.json` is symlinked to `build/debug/` for clangd; re-run the configure step if
it goes stale.
