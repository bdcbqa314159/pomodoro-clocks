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
| `q` / `esc` | quit |

Durations are the defaults (25 / 5 / 15, long break every 4). The core supports changing them;
the TUI has no settings screen yet.

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
