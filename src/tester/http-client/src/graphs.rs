use std::{
    collections::{HashMap, HashSet, VecDeque},
    sync::{mpsc::Sender, Arc},
};

use egui::{
    epaint::CubicBezierShape, Color32, Grid, Id, InnerResponse, Label, Layout, Margin, Pos2, Rect,
    RichText, Sense, Stroke, TextStyle, Ui, Vec2,
};
use reqwest::Client;

use crate::{
    app::{Relation, RelationType},
    handle::Handle,
    http,
};

#[derive(Default)]
pub(crate) struct Graph {
    forward: HashMap<Handle, HashSet<Relation>>,
    backward: HashMap<Handle, HashSet<Relation>>,
}

impl Graph {
    pub(crate) fn insert(&mut self, relation: Relation) {
        self.forward
            .entry(relation.lhs.clone())
            .or_default()
            .insert(relation.clone());
        self.backward
            .entry(relation.rhs.clone())
            .or_default()
            .insert(relation.clone());
    }

    pub(crate) fn visit_bfs(&self, root: Handle, mut handle: impl FnMut(&Relation)) {
        fn handle_relations(
            relations: &HashSet<Relation>,
            to_visit: &mut VecDeque<Handle>,
            seen: &mut HashSet<Handle>,
            handle: &mut impl FnMut(&Relation),
            selector: impl Fn(&Relation) -> &Handle,
        ) {
            for r in relations {
                handle(r);
                let target = selector(r);
                if !seen.contains(target) {
                    to_visit.push_back(target.clone());
                    seen.insert(target.clone());
                }
            }
        }
        let mut seen = HashSet::new();
        let mut to_visit: VecDeque<_> = vec![root].into();
        while let Some(next) = to_visit.pop_front() {
            if let Some(relations) = self.forward.get(&next) {
                handle_relations(relations, &mut to_visit, &mut seen, &mut handle, |r| &r.rhs);
            }
            if let Some(relations) = self.backward.get(&next) {
                handle_relations(relations, &mut to_visit, &mut seen, &mut handle, |r| &r.lhs);
            }
        }
    }
}

pub(crate) struct Ports {
    pub input: Pos2,
    pub output: Pos2,
}

fn add_object(
    ctx: &egui::Context,
    id: impl std::hash::Hash,
    handle: Handle,
    start_pos: Pos2,
    add_contents: impl FnOnce(&mut Ui) -> f32,
) -> Ports {
    fn add_dot(ui: &mut Ui, center: Pos2) {
        ui.allocate_rect(
            Rect::from_center_size(center, Vec2::new(10.0, 10.0)),
            Sense::click_and_drag(),
        );
        ui.painter()
            .circle(center, 4.0, Color32::WHITE, Stroke::NONE);
    }
    egui::containers::Area::new(Id::new(id))
        .default_pos(start_pos)
        .movable(true)
        .show(ctx, |ui| {
            ui.with_layout(Layout::default().with_main_wrap(false), |ui| {
                ui.style_mut().wrap = Some(false);
                let InnerResponse { inner, response } = egui::Frame::default()
                    .rounding(egui::Rounding::same(4.0))
                    .inner_margin(Margin::same(8.0))
                    .stroke(ctx.style().visuals.window_stroke)
                    .fill(ui.style().visuals.panel_fill)
                    .show(ui, |ui| {
                        ui.set_max_size(Vec2::new(200.0, f32::INFINITY));
                        ui.add(Label::new(
                            // TODO use nice print of id
                            RichText::new(handle.to_string())
                                .text_style(TextStyle::Button)
                                .color(ui.style().visuals.strong_text_color()),
                        ));
                        add_contents(ui)
                    });
                let title_center = response.rect.center().y;
                let dot_center = Pos2::new(ui.min_rect().left(), title_center);
                add_dot(ui, dot_center);
                let out_dot_center = Pos2::new(ui.min_rect().right(), inner);
                add_dot(ui, out_dot_center);
                Ports {
                    input: dot_center,
                    output: out_dot_center,
                }
            })
            .inner
        })
        .inner
}

pub(crate) fn add_main_node(
    client: &mut Arc<Client>,
    ctx: &egui::Context,
    tx: Sender<std::result::Result<Vec<Relation>, anyhow::Error>>,
    handle: Handle,
    target_input: &mut String,
    error: &str,
) -> Ports {
    add_object(
        ctx,
        "main object",
        handle.clone(),
        Pos2::new(20.0, 20.0),
        |ui| {
            Grid::new(handle.to_hex() + " properties")
                .num_columns(2)
                .show(ui, |ui| {
                    let start_y = ui.min_rect().bottom();
                    ui.label("Handle:");
                    ui.text_edit_singleline(target_input);
                    // ui.add(Label::new(handle.to_hex()).truncate(true))
                    //     .on_hover_cursor(egui::CursorIcon::Copy);
                    ui.end_row();
                    let handle_middle_height = (ui.min_rect().bottom() + start_y) / 2.0;
                    if ui.button("load").clicked() {
                        http::get_explanations(
                            client.clone(),
                            ctx.clone(),
                            &handle,
                            tx,
                            "127.0.0.1:9090",
                        )
                    }
                    ui.end_row();
                    ui.label("Error: ");
                    ui.label(error);
                    ui.end_row();

                    handle_middle_height
                })
                .inner
        },
    )
}
// TODO For now, just add every node from every relation.
// Just connect the first out port and first in port.

pub(crate) fn add_node(
    client: &mut Arc<Client>,
    ctx: &egui::Context,
    tx: Sender<std::result::Result<Vec<Relation>, anyhow::Error>>,
    handle: Handle,
) -> Ports {
    add_object(
        ctx,
        handle.clone(),
        handle.clone(),
        Pos2::new(20.0, 20.0),
        |ui| {
            Grid::new(handle.to_hex() + " properties")
                .num_columns(2)
                .show(ui, |ui| {
                    let start_y = ui.min_rect().bottom();
                    ui.label("Handle:");
                    if ui
                        .add(
                            Label::new(handle.to_hex())
                                .truncate(true)
                                .sense(Sense::click()),
                        )
                        .on_hover_cursor(egui::CursorIcon::Copy)
                        .clicked()
                    {
                        ui.output_mut(|o| o.copied_text = handle.to_hex())
                    };
                    ui.end_row();
                    let handle_middle_height = (ui.min_rect().bottom() + start_y) / 2.0;

                    if ui.button("load").clicked() {
                        http::get_explanations(
                            client.clone(),
                            ctx.clone(),
                            &handle,
                            tx,
                            "127.0.0.1:9090",
                        )
                    }

                    handle_middle_height
                })
                .inner
        },
    )
}

fn get_bezier(
    src: Pos2,
    src_dir: Vec2,
    dst: Pos2,
    dst_dir: Vec2,
    color: Color32,
) -> CubicBezierShape {
    let connection_stroke = egui::Stroke { width: 5.0, color };

    let control_scale = ((dst.x - src.x) / 2.0).max(30.0);
    let src_control = src + src_dir * control_scale;
    let dst_control = dst + dst_dir * control_scale;

    CubicBezierShape::from_points_stroke(
        [src, src_control, dst_control, dst],
        false,
        Color32::TRANSPARENT,
        connection_stroke,
    )
}

pub(crate) fn get_connection(
    src: Pos2,
    dst: Pos2,
    relation_type: RelationType,
    is_self_loop: bool,
) -> CubicBezierShape {
    let (src_dir, dst_dir) = if is_self_loop {
        (5.0 * (Vec2::X + Vec2::Y), -5.0 * (Vec2::X + Vec2::Y))
    } else {
        (Vec2::X, -Vec2::X)
    };
    let color = match relation_type {
        RelationType::Eval => Color32::BLUE,
        RelationType::Apply => Color32::GREEN,
        RelationType::Fill => Color32::RED,
    };
    get_bezier(src, src_dir, dst, dst_dir, color)
}
