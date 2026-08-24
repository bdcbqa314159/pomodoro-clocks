//! Sound + desktop notification. The only platform-specific code in the crate.

#[cfg(not(target_arch = "wasm32"))]
pub fn request_permission() {}

#[cfg(not(target_arch = "wasm32"))]
pub fn alert(title: &str, body: &str) {
    // ponytail: shell out, exactly like the C++ build does. A native
    // notification binding wants a signed .app bundle to deliver two lines.
    #[cfg(target_os = "macos")]
    {
        let script = format!(
            "display notification {} with title {} sound name \"Glass\"",
            applescript_quote(body),
            applescript_quote(title)
        );
        let _ = std::process::Command::new("osascript")
            .arg("-e")
            .arg(script)
            .spawn(); // spawn, not status: never block the UI thread
    }
    #[cfg(not(target_os = "macos"))]
    {
        // ponytail: no notification daemon assumed. Wire notify-rust if you
        // want this on Linux/Windows.
        let _ = (title, body);
        eprint!("\x07");
    }
}

/// AppleScript string literal: wrap in quotes, escape backslashes and quotes.
/// Titles are compile-time constants today, but this is the boundary where a
/// future dynamic string would otherwise become script injection.
#[cfg(all(not(target_arch = "wasm32"), target_os = "macos"))]
fn applescript_quote(s: &str) -> String {
    let escaped = s.replace('\\', "\\\\").replace('"', "\\\"");
    format!("\"{escaped}\"")
}

// ---------------------------------------------------------------------------

#[cfg(target_arch = "wasm32")]
pub fn request_permission() {
    if web_sys::Notification::permission() == web_sys::NotificationPermission::Default {
        // Fire-and-forget: the browser resolves the promise, we do not care when.
        let _ = web_sys::Notification::request_permission();
    }
}

#[cfg(target_arch = "wasm32")]
pub fn alert(title: &str, body: &str) {
    chime(title.starts_with("Focus"));

    if web_sys::Notification::permission() == web_sys::NotificationPermission::Granted {
        let _ = web_sys::Notification::new(&format!("{title} — {body}"));
    }
}

/// Rising triad ending focus, falling one ending a break. Synthesised, so there
/// is no audio file to ship or preload.
#[cfg(target_arch = "wasm32")]
fn chime(up: bool) {
    let Ok(ctx) = web_sys::AudioContext::new() else {
        return;
    };
    let freqs: [f32; 3] = if up {
        [523.0, 659.0, 784.0]
    } else {
        [784.0, 659.0, 523.0]
    };
    for (i, f) in freqs.iter().enumerate() {
        let at = ctx.current_time() + f64::from(i as u32) * 0.16;
        let _ = tone(&ctx, *f, at, 1.1);
    }
}

#[cfg(target_arch = "wasm32")]
fn tone(ctx: &web_sys::AudioContext, freq: f32, at: f64, dur: f64) -> Result<(), wasm_bindgen::JsValue> {
    let osc = ctx.create_oscillator()?;
    let gain = ctx.create_gain()?;
    osc.set_type(web_sys::OscillatorType::Sine);
    osc.frequency().set_value(freq);
    gain.gain().set_value_at_time(0.0, at)?;
    gain.gain().linear_ramp_to_value_at_time(0.22, at + 0.02)?;
    gain.gain().exponential_ramp_to_value_at_time(0.0001, at + dur)?;
    osc.connect_with_audio_node(&gain)?;
    gain.connect_with_audio_node(&ctx.destination())?;
    osc.start_with_when(at)?;
    osc.stop_with_when(at + dur)?;
    Ok(())
}
