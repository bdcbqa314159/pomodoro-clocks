# pomodoro

A pomodoro timer, three ways.

| flavour | stack | run it |
|---|---|---|
| **Terminal** | C++20 + FTXUI | `pomodoro` |
| **Desktop GUI** | Rust + egui | `cd rust && cargo run --release` |
| **Browser** | Rust + egui → WASM | `cd rust && trunk serve --release` |

The GUI and the browser build are **the same Rust code** — one crate, two targets. `src/app.rs`
is compiled unchanged for both; only the entry point in `src/main.rs` differs.

There is **no JavaScript** in this project. The browser build ships a wasm-bindgen loader that
Trunk generates; no `.js` file is hand-written or maintained.

`prototype/index.html` is the original HTML/JS version, kept as a reference.

## Prerequisites

Only what the flavour you want needs:

| for | need |
|---|---|
| Terminal (C++) | CMake ≥ 3.21, Ninja, a C++20 compiler |
| Desktop GUI (Rust) | a Rust toolchain |
| Browser (Rust → WASM) | the above, plus `rustup target add wasm32-unknown-unknown` and `cargo install trunk` |

Everything else — FTXUI, GoogleTest, egui — is fetched by the build. Nothing to install by hand,
and nothing installed globally.

## Install

```sh
cmake --preset release
cmake --build --preset release
cmake --install build/release
```

Installs `bin/pomodoro` to `~/.local` by default — **no `sudo`, nothing touched outside your
home directory**. Add `~/.local/bin` to `PATH` if it isn't already:

```sh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc
```

Anywhere else:

```sh
cmake --install build/release --prefix /opt/tools      # or any writable directory
```

Only the executable is installed. To uninstall, delete `<prefix>/bin/pomodoro`.

## Run

```sh
pomodoro                      # if installed
./build/release/pomodoro      # straight from the build tree
```

| key | |
|---|---|
| `space` | start / pause |
| `r` | reset the current phase |
| `s` | skip to the next phase |
| `c` | settings (arrows to select/adjust, `esc` to close) |
| `q` / `esc` | quit |

Defaults are 25 / 5 / 15 with a long break every 4. Changing them in the settings screen writes
a config file on close — resolved in this order:

1. `$XDG_CONFIG_HOME/pomodoro/config`
2. `~/.config/pomodoro/config`
3. `%APPDATA%\pomodoro\config` (Windows)
4. none of the above set → runs on defaults and persists nothing, saying so on screen

The config belongs to the user, not the binary: move, reinstall or rebuild the executable and
your settings follow you.

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
src/pomodoro.hpp     C++ timer core — pure logic, no I/O, no clock reads
src/pomodoro.cpp     implementation
src/config_io.*      config file read/write — parsing half is pure and tested
src/main.cpp         FTXUI terminal app — the only place that reads a clock
tests/               GoogleTest, fake time
docs/                milestone notes
prototype/           the HTML original

rust/src/timer.rs    Rust timer core — same semantics, f64 seconds not Instant
rust/src/app.rs      egui UI — shared by the native GUI and the browser build
rust/src/alert.rs    sound + notifications; the only cfg-split file
rust/src/main.rs     both entry points (native window / wasm canvas)
rust/index.html      Trunk template — markup only, no script tag
```

## Rust builds

```sh
cd rust
cargo test              # 20 core tests, ported from the C++ suite
cargo run --release     # native GUI window
trunk serve --release   # browser at http://127.0.0.1:8080
trunk build --release   # static bundle in rust/dist/
```

The wasm bundle is ~3.9 MB. `wasm-opt` needs `--enable-bulk-memory` (set in `rust/index.html`)
because the toolchain emits `memory.copy`.

Settings persist in both Rust builds through eframe's storage — a file on native,
`localStorage` in the browser. They are **not** shared with the C++ build's
`~/.config/pomodoro/config`; the three flavours keep their own settings.

## Dev tooling

Pinned locally, never installed globally:

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements-dev.txt
.venv/bin/clang-format -i $(git ls-files '*.hpp' '*.cpp')
```

`compile_commands.json` is symlinked to `build/debug/` for clangd; re-run the configure step if
it goes stale.
