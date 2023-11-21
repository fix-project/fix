#![warn(clippy::all, rust_2018_idioms)]
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")] // hide console window on Windows in release

// When compiling natively:
#[cfg(not(target_arch = "wasm32"))]
fn main() -> eframe::Result<()> {
    use tokio::runtime::Runtime;

    env_logger::init(); // Log to stderr (if you run with `RUST_LOG=debug`).

    // Initialize a tokio runtime.
    let rt = Runtime::new().unwrap();
    let native_options = eframe::NativeOptions::default();
    rt.block_on(async move {
        eframe::run_native(
            "Fix Viewer",
            native_options,
            Box::new(|cc| Box::new(fix_viewer::App::new(cc))),
        )
    })
}

// When compiling to web using trunk:
#[cfg(target_arch = "wasm32")]
fn main() {
    // Redirect `log` message to `console.log` and friends:
    eframe::WebLogger::init(log::LevelFilter::Trace).ok();

    let web_options = eframe::WebOptions::default();

    wasm_bindgen_futures::spawn_local(async {
        eframe::WebRunner::new()
            .start(
                "the_canvas_id", // hardcode it
                web_options,
                Box::new(|cc| Box::new(fix_viewer::App::new(cc))),
            )
            .await
            .expect("failed to start eframe");
    });
}
