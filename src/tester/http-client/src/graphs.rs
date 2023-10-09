use std::sync::mpsc::Sender;
use std::sync::Arc;

use anyhow::Result;
use egui::plot::Plot;
use egui::Context;
use egui::{plot::items::PlotItem, Ui};

use crate::handle::{Operation, Task};
use crate::http;
use crate::{handle::Handle, plot::Element};

mod ancestors;
mod progress;

#[derive(Clone)]
pub(crate) struct GraphsContainer {
    ancestry: ancestors::AncestorGraph,
    progress: progress::ProgressGraph,
}

impl GraphsContainer {
    pub fn new(ui: &Ui, handle: Handle, operation: Operation) -> Self {
        Self {
            ancestry: ancestors::AncestorGraph::new(Element::new(ui, handle.clone())),
            progress: progress::ProgressGraph::new(Element::new(ui, handle), operation),
        }
    }

    pub fn view(
        &self,
        ctx: &Context,
        client: Arc<reqwest::Client>,
        url: &str,
        tx: Sender<(usize, Handle, Result<http::Response>)>,
    ) {
        egui::Window::new("Ancestry Tree").resizable(true).show(ctx, |ui| {
            let hovered_elem = Plot::new("ancestry_plot")
                .data_aspect(1.0)
                .auto_bounds_x()
                .auto_bounds_y()
                .show_axes([false; 2])
                .show_x(false)
                .show_y(false)
                .show(ui, |plot_ui| {
                    let graph = &self.ancestry;
                    plot_ui.add(graph.clone());
                    let (Some(coords), true) = (plot_ui.pointer_coordinate(), plot_ui.plot_clicked()) else {
                        return None
                    };
                    let closest_elem = graph
                        .find_closest(plot_ui.screen_from_plot(coords), plot_ui.transform())?;
                    Some((coords, closest_elem))
                }).inner;

            if let Some((coords, closest_elem)) = hovered_elem {
                    self.ancestry.handle_nearby_click(ui, coords, closest_elem, |index, handle| {
                        http::get_parents(
                            client.clone(),
                            ctx.clone(),
                            index,
                            handle,
                            tx.clone(),
                            url,
                        );
                    });
            }
        });
        egui::Window::new("Progress Tree").resizable(true).show(ctx, |ui| {
            let hovered_elem = Plot::new("progress_plot")
                .data_aspect(1.0)
                .auto_bounds_x()
                .auto_bounds_y()
                .show_axes([false; 2])
                .show_x(false)
                .show_y(false)
                .show(ui, |plot_ui| {
                    let graph = &self.progress;
                    plot_ui.add(graph.clone());
                    let (Some(coords), true) = (plot_ui.pointer_coordinate(), plot_ui.plot_clicked()) else {
                        return None
                    };
                    let closest_elem = graph
                        .find_closest(plot_ui.screen_from_plot(coords), plot_ui.transform())?;
                    Some((coords, closest_elem))
                }).inner;

            if let Some((coords, closest_elem)) = hovered_elem {
                    self.progress.handle_nearby_click(ui, coords, closest_elem, |index, handle, operation| {
                        http::get_child(
                            client.clone(),
                            ctx.clone(),
                            index,
                            handle.clone(),
                            operation,
                            tx.clone(),
                            url,
                        );

                        // hack around http web server only handling one request in a short time
                        let ctx_clone = ctx.clone();
                        let url_clone = url.to_string();
                        #[cfg(target_arch = "wasm32")]
                        {
                            use wasm_bindgen::JsCast;
                            let window = web_sys::window().expect("Missing window.");
                             let _ = window.set_timeout_with_callback_and_timeout_and_arguments_0(
                                wasm_bindgen::closure::Closure::once_into_js(move ||
                                 http::get_dependees(
                                    client.clone(),
                                    ctx_clone,
                                    index,
                                    handle,
                                    operation,
                                    tx.clone(),
                                    &url_clone,
                                )
                                ).as_ref().unchecked_ref(),
                                100,
                            );
                        }
                        #[cfg(not(target_arch = "wasm32"))]
                        let _ = tokio::spawn(async {
                            tokio::time::sleep(Duration::from_millis(100)).await;
                            http::get_dependees(
                                client.clone(),
                                ctx.clone(),
                                index,
                                handle,
                                operation,
                                tx.clone(),
                                url,
                            );
                        });
                    });
            }
        });
    }

    /// Set the parents of a specific handle
    pub fn set_parents(&mut self, ui: &Ui, handle: Handle, parents: Vec<Task>) {
        // Merge into the ancestry tree.
        self.ancestry.merge_new_parents(ui, handle, &parents);
    }

    pub fn set_child(&mut self, ui: &Ui, index: usize, child: Handle) {
        self.progress.set_child(ui, index, child);
    }

    pub fn merge_dependees(&mut self, ui: &Ui, index: usize, dependees: Vec<Task>) {
        log::error!("{:?}", dependees);
        self.progress.merge_dependees(ui, index, dependees);
    }

    pub(crate) fn set_operation(&mut self, ui: &Ui, operation: Operation, handle: Handle) {
        self.progress = progress::ProgressGraph::new(Element::new(ui, handle), operation);
    }
}
