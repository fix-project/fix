use std::{
    collections::HashMap,
    sync::{
        mpsc::{channel, Receiver, Sender},
        Arc,
    },
};

use anyhow::Result;
use egui::Visuals;
use reqwest::Client;

use crate::{
    graphs::{add_main_node, add_node, get_connection, Graph, Ports},
    handle::Handle,
};

pub struct App {
    state: State,
    storage: Storage,
}

/// We derive Deserialize/Serialize so we can persist app state on shutdown.
#[derive(serde::Deserialize, serde::Serialize)]
#[serde(default)] // if we add new fields, give them default values when deserializing old state
struct Storage {
    target: Handle,
}

impl Default for Storage {
    fn default() -> Self {
        Self {
            target: Handle::from_hex(
                "1000000000000000000000000000000000000000000000000000000000000024",
            )
            .unwrap(),
        }
    }
}

struct State {
    target_input: String,
    error: Error,
    first_render: bool,
    client: Arc<Client>,
    response_tx: Sender<Result<Vec<Relation>>>,
    response_rx: Receiver<Result<Vec<Relation>>>,
    connections: Graph,
}

#[derive(Hash, PartialEq, Eq, Clone)]
pub(crate) struct Relation {
    pub(crate) lhs: Handle,
    pub(crate) rhs: Handle,
    pub(crate) relation_type: RelationType,
}

// For now. Should add content, tag.
#[derive(Hash, PartialEq, Eq, Clone, Copy)]
pub(crate) enum RelationType {
    Eval,
    Apply,
    Fill,
}

#[derive(Default)]
struct Error {
    content: String,
    dirty: bool,
}

impl Error {
    fn write(&mut self, content: String) {
        self.dirty = true;
        self.content = content;
    }

    fn read_and_update(&mut self) -> &str {
        if self.dirty {
            self.dirty = false;
            &self.content
        } else {
            ""
        }
    }
}

impl Default for State {
    fn default() -> Self {
        let (tx, rx) = channel();
        Self {
            target_input: "1000000000000000000000000000000000000000000000000000000000000024"
                .to_owned(),
            error: Error::default(),
            first_render: true,
            client: Arc::new(Client::new()),
            response_tx: tx,
            response_rx: rx,
            connections: Graph::default(),
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
            error,
            first_render,
            client,
            response_tx: tx,
            response_rx: rx,
            connections,
        } = &mut self.state;

        if let Ok(new_connections) = rx.try_recv() {
            match new_connections {
                Ok(new_connections) => {
                    for c in new_connections {
                        connections.insert(c);
                    }
                }
                Err(e) => error.write(format!("{:#}", e)),
            }
        }

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Objects");
            ui.separator();

            let mut handle_to_ports: HashMap<Handle, Ports> = HashMap::new();
            let painter = ui.painter();

            let main_handle = match Handle::from_hex(target_input) {
                Ok(h) => {
                    storage.target = h.clone();
                    h
                }
                Err(e) => {
                    error.write(format!("{:#}", e));
                    storage.target.clone()
                }
            };

            handle_to_ports.insert(
                main_handle.clone(),
                add_main_node(
                    client,
                    ctx,
                    tx.clone(),
                    main_handle.clone(),
                    target_input,
                    error.read_and_update(),
                ),
            );

            connections.visit_bfs(main_handle.clone(), |connection| {
                let out_port = handle_to_ports
                    .entry(connection.lhs.clone())
                    .or_insert_with(|| add_node(client, ctx, tx.clone(), connection.lhs.clone()))
                    .output;
                let in_port = handle_to_ports
                    .entry(connection.rhs.clone())
                    .or_insert_with(|| add_node(client, ctx, tx.clone(), connection.rhs.clone()))
                    .input;

                painter.add(get_connection(
                    out_port,
                    in_port,
                    connection.relation_type,
                    connection.lhs == connection.rhs,
                ));
            });
        });

        *first_render = false;
    }
}
