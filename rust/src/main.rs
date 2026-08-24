// No console window on Windows release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use pomodoro::app::PomodoroApp;

#[cfg(not(target_arch = "wasm32"))]
fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default()
            .with_inner_size([420.0, 620.0])
            .with_min_inner_size([320.0, 480.0])
            .with_title("Pomodoro"),
        ..Default::default()
    };
    eframe::run_native(
        "Pomodoro",
        options,
        Box::new(|cc| Ok(Box::new(PomodoroApp::new(cc)))),
    )
}

#[cfg(target_arch = "wasm32")]
fn main() {
    use wasm_bindgen::JsCast as _;

    wasm_bindgen_futures::spawn_local(async {
        let canvas = web_sys::window()
            .expect("no window")
            .document()
            .expect("no document")
            .get_element_by_id("canvas")
            .expect("index.html is missing <canvas id=\"canvas\">")
            .dyn_into::<web_sys::HtmlCanvasElement>()
            .expect("#canvas is not a <canvas>");

        eframe::WebRunner::new()
            .start(
                canvas,
                eframe::WebOptions::default(),
                Box::new(|cc| Ok(Box::new(PomodoroApp::new(cc)))),
            )
            .await
            .expect("failed to start eframe");
    });
}
