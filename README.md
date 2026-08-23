# pomodoro

A pomodoro timer, as a terminal app. C++20.

`prototype/index.html` is the working reference implementation — open it in a browser. The C++
port exists to rebuild it as a native TUI (and to learn C++ doing it).

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, `asan` (non-Windows).

## Layout

```
src/pomodoro.hpp     timer core — pure logic, no I/O, no clock reads
src/pomodoro.cpp     implementation
tests/               GoogleTest, fake time
docs/                milestone tasks
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
