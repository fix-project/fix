use std::sync::{
    mpsc::{channel, Receiver, Sender},
    Arc,
};

use anyhow::Result;
use egui::{TextEdit, Visuals};
use reqwest::Client;

use crate::{
    graphs::GraphsContainer,
    handle::{Handle, Operation},
    http::{self, Response},
};

pub struct App {
    state: State,
    storage: Storage,
}

/// We derive Deserialize/Serialize so we can persist app state on shutdown.
#[derive(serde::Deserialize, serde::Serialize)]
#[serde(default)] // if we add new fields, give them default values when deserializing old state
struct Storage {
    url: String,
    target: Handle,
    operation: Operation,
}

impl Default for Storage {
    fn default() -> Self {
        Self {
            url: String::new(),
            target: Handle::from_hex("0-0-0-2400000000000000").unwrap(),
            operation: Operation::Eval,
        }
    }
}

struct State {
    target_input: String,
    response: String,
    error: String,
    first_render: bool,
    client: Arc<Client>,
    response_tx: Sender<(usize, Handle, Result<http::Response>)>,
    response_rx: Receiver<(usize, Handle, Result<http::Response>)>,
    graph: Option<GraphsContainer>,
}

impl Default for State {
    fn default() -> Self {
        let (tx, rx) = channel();
        Self {
            target_input: String::new(),
            response: String::new(),
            error: String::new(),
            first_render: true,
            client: Arc::new(Client::new()),
            response_tx: tx,
            response_rx: rx,
            graph: None,
        }
    }
}

impl App {
    /// Called once before the first frame.
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        // Load previous app state (if any).
        // Note that you must enable the `persistence` feature for this to work.
        if let Some(storage) = cc.storage {
            return Self {
                state: State::default(),
                storage: eframe::get_value(storage, eframe::APP_KEY).unwrap_or_default(),
            };
        }

        cc.egui_ctx.set_visuals(Visuals::dark());

        App {
            state: State::default(),
            storage: Storage::default(),
        }
    }
}

impl eframe::App for App {
    /// Called by the frame work to save state before shutdown.
    fn save(&mut self, storage: &mut dyn eframe::Storage) {
        eframe::set_value(storage, eframe::APP_KEY, &self.storage);
    }

    /// Called each time the UI needs repainting, which may be many times per second.
    /// Put your widgets into a `SidePanel`, `TopPanel`, `CentralPanel`, `Window` or `Area`.
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let storage = &mut self.storage;
        let State {
            target_input,
            response,
            error,
            first_render,
            client,
            response_tx: tx,
            response_rx: rx,
            graph,
        } = &mut self.state;

        #[cfg(not(target_arch = "wasm32"))] // no File->Quit on web pages!
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            egui::menu::bar(ui, |ui| {
                ui.menu_button("File", |ui| {
                    if ui.button("Quit").clicked() {
                        _frame.close();
                    }
                });
            });
        });

        egui::SidePanel::left("controls").show(ctx, |ui| {
            ui.heading("Controls");

            ui.separator();

            ui.horizontal(|ui| {
                ui.label("URL: ");
                TextEdit::singleline(&mut storage.url)
                    .hint_text("127.0.0.1:9090")
                    .desired_width(f32::INFINITY)
                    .show(ui);
            });

            if *first_render {
                *target_input = storage.target.to_hex();
            }
            ui.horizontal(|ui| {
                if *first_render {
                    *graph = Some(GraphsContainer::new(
                        ui,
                        storage.target.clone(),
                        storage.operation,
                    ));
                }
                ui.label("Target: ");
                if TextEdit::singleline(target_input)
                    .desired_width(f32::INFINITY)
                    .show(ui)
                    .response
                    .changed()
                    || *first_render
                {
                    match Handle::from_hex(target_input) {
                        Ok(handle) => {
                            error.clear();
                            storage.target = handle.clone();
                            *graph = Some(GraphsContainer::new(ui, handle, storage.operation));
                        }
                        Err(e) => *error = format!("{:#}", e),
                    }
                }
            });

            let operation = storage.operation;
            ui.selectable_value(
                &mut storage.operation,
                Operation::Eval,
                Operation::Eval.to_string(),
            );
            ui.selectable_value(
                &mut storage.operation,
                Operation::Apply,
                Operation::Apply.to_string(),
            );
            ui.selectable_value(
                &mut storage.operation,
                Operation::Fill,
                Operation::Fill.to_string(),
            );
            if operation != storage.operation {
                graph.as_mut().unwrap().set_operation(
                    ui,
                    storage.operation,
                    storage.target.clone(),
                );
            }

            if let Ok(http_result) = rx.try_recv() {
                let index = http_result.0;
                let handle = http_result.1;
                match http_result.2 {
                    Ok(Response::Parents(tasks)) => {
                        if let Some(tasks) = tasks {
                            log::info!("Received tasks {:?}", tasks);
                            graph.as_mut().unwrap().set_parents(ui, handle, tasks);
                        }
                    }
                    Ok(Response::Child(child)) => {
                        if let Some(child) = child {
                            graph.as_mut().unwrap().set_child(ui, index, child);
                        }
                    }
                    Ok(Response::Dependees(tasks)) => {
                        log::error!("received response tasks, {:?}", tasks);
                        if let Some(tasks) = tasks {
                            graph.as_mut().unwrap().merge_dependees(ui, index, tasks);
                        }
                    }
                    Err(e) => *error = format!("Failed http request: {}.", e.root_cause()),
                }
            }

            ui.separator();
            ui.colored_label(Operation::Apply.get_color(), Operation::Apply.to_string());
            ui.colored_label(Operation::Eval.get_color(), Operation::Eval.to_string());
            ui.colored_label(Operation::Fill.get_color(), Operation::Fill.to_string());
            ui.separator();
            ui.label(response.as_str());
            ui.label(error.as_str());

            ui.with_layout(egui::Layout::bottom_up(egui::Align::LEFT), |ui| {
                ui.horizontal(|ui| {
                    ui.spacing_mut().item_spacing.x = 0.0;
                    ui.label("powered by ");
                    ui.hyperlink_to("egui", "https://github.com/emilk/egui");
                    ui.label(" and ");
                    ui.hyperlink_to(
                        "eframe",
                        "https://github.com/emilk/egui/tree/master/crates/eframe",
                    );
                    ui.label(".");
                });
            });
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Windows");
            ui.separator();
        });
        graph
            .as_mut()
            .unwrap()
            .view(ctx, client.clone(), &storage.url, tx.clone());
        *first_render = false;
    }
}
