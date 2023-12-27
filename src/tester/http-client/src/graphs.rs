use std::{
    borrow::Cow,
    collections::{BTreeMap, HashMap, HashSet, VecDeque},
    fmt::Display,
};

use egui::{
    epaint::CubicBezierShape, Color32, Grid, Id, InnerResponse, Label, Layout, Margin, Pos2, Rect,
    RichText, Sense, Stroke, TextStyle, Ui, Vec2,
};

use crate::{
    handle::{Handle, Object},
    http::{self, HttpContext},
};

#[derive(Default)]
pub(crate) struct Graph {
    forward: HashMap<Handle, BTreeMap<RelationType, HashSet<Relation>>>,
    backward: HashMap<Handle, BTreeMap<RelationType, HashSet<Relation>>>,
}

impl Graph {
    pub(crate) fn insert(&mut self, relation: Relation) {
        self.forward
            .entry(relation.lhs.clone())
            .or_default()
            .entry(relation.relation_type)
            .or_default()
            .insert(relation.clone());
        self.backward
            .entry(relation.rhs.clone())
            .or_default()
            .entry(relation.relation_type)
            .or_default()
            .insert(relation);
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
                for relation_set in relations.values() {
                    handle_relations(relation_set, &mut to_visit, &mut seen, &mut handle, |r| {
                        &r.rhs
                    });
                }
            }
            if let Some(relations) = self.backward.get(&next) {
                for relation_set in relations.values() {
                    handle_relations(relation_set, &mut to_visit, &mut seen, &mut handle, |r| {
                        &r.lhs
                    });
                }
            }
        }
    }
}

#[derive(Hash, PartialEq, Eq, Clone, Debug)]
pub(crate) struct Relation {
    pub(crate) lhs: Handle,
    pub(crate) rhs: Handle,
    pub(crate) relation_type: RelationType,
}

impl Relation {
    pub(crate) fn new(lhs: Handle, rhs: Handle, relation_type: RelationType) -> Self {
        Self {
            lhs,
            rhs,
            relation_type,
        }
    }
}

// For now. Should add content, tag.
/// The order of these fields dictates the order in which they show up in the
/// visualization windows.
#[derive(Hash, PartialEq, Eq, Clone, Copy, Debug, PartialOrd, Ord)]
pub(crate) enum RelationType {
    Eval,
    Apply,
    Fill,
    Pin,
    TagAuthor,
    TagTarget,
    TagLabel,
    TreeEntry(usize),
}

impl Display for RelationType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let name = match self {
            Self::Eval => Cow::Borrowed("evaluates into"),
            Self::Apply => Cow::Borrowed("applies into"),
            Self::Fill => Cow::Borrowed("fills into"),
            Self::Pin => Cow::Borrowed("pins"),
            Self::TagAuthor => Cow::Borrowed("this object"),
            Self::TagTarget => Cow::Borrowed("tags this object"),
            Self::TagLabel => Cow::Borrowed("with this label"),
            Self::TreeEntry(i) => Cow::Owned(format!("has entry at index [{}]", i)),
        };
        f.write_fmt(format_args!("{}", name))
    }
}

impl RelationType {
    fn get_color(&self) -> Color32 {
        match self {
            RelationType::Eval => Color32::BLUE,
            RelationType::Apply => Color32::GREEN,
            RelationType::Fill => Color32::RED,
            RelationType::Pin => Color32::GRAY,
            RelationType::TagTarget => Color32::LIGHT_BLUE,
            RelationType::TagAuthor => Color32::LIGHT_GREEN,
            RelationType::TagLabel => Color32::LIGHT_RED,
            RelationType::TreeEntry(_) => Color32::GRAY,
        }
    }
}

pub(crate) struct Ports {
    pub input: Pos2,
    pub outputs: HashMap<RelationType, Pos2>,
}

fn add_object(
    ctx: &egui::Context,
    window_id: impl std::hash::Hash,
    handle: Handle,
    start_pos: Pos2,
    forward_relations: Option<&BTreeMap<RelationType, HashSet<Relation>>>,
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

    fn main_body<'a>(
        ui: &mut Ui,
        handle: Handle,
        add_contents: impl FnOnce(&mut Ui) -> f32,
        forward_relations: Option<&'a BTreeMap<RelationType, HashSet<Relation>>>,
    ) -> HashMap<&'a RelationType, f32> {
        ui.add(Label::new(
            RichText::new(format!(
                "{}, {}",
                handle.get_content_type(),
                handle.get_identifier()
            ))
            .text_style(TextStyle::Button)
            .color(ui.style().visuals.strong_text_color()),
        ));
        add_contents(ui);
        ui.separator();
        let mut ports = HashMap::new();
        if let Some(relations) = forward_relations {
            for relation_type in relations.keys() {
                // Sorted by relation type.
                let start_height = ui.min_rect().bottom();
                ui.label(relation_type.to_string());
                let end_height = ui.min_rect().bottom();
                ui.end_row();
                ports.insert(relation_type, (start_height + end_height) / 2.0);
            }
        }
        ports
    }

    // Note that the window_id should not be derived from the handle.
    // This allows the "main" window with an editable handle to not
    // jump around while the user types into it.
    egui::containers::Area::new(Id::new(window_id))
        .default_pos(start_pos)
        .movable(true)
        .show(ctx, |ui| {
            ui.with_layout(Layout::default().with_main_wrap(false), |ui| {
                // ui.style_mut().wrap = Some(false);
                let InnerResponse { inner, response } = egui::Frame::default()
                    .rounding(egui::Rounding::same(4.0))
                    .inner_margin(Margin::same(8.0))
                    .stroke(ctx.style().visuals.window_stroke)
                    .fill(ui.style().visuals.panel_fill)
                    .show(ui, |ui| {
                        egui::containers::Resize::default()
                            .id((handle.to_hex() + " resizable window").into())
                            .with_stroke(false)
                            .show(ui, |ui| {
                                main_body(ui, handle, add_contents, forward_relations)
                            })
                    });
                let window_center = response.rect.center().y;
                let dot_center = Pos2::new(ui.min_rect().left(), window_center);
                add_dot(ui, dot_center);

                let outputs: HashMap<_, _> = inner
                    .into_iter()
                    .map(|(r_type, height)| (*r_type, Pos2::new(ui.min_rect().right(), height)))
                    .collect();
                for pos in outputs.values() {
                    add_dot(ui, *pos);
                }

                Ports {
                    input: dot_center,
                    outputs,
                }
            })
            .inner
        })
        .inner
}

fn add_fetch_buttons(ui: &mut Ui, ctx: HttpContext, handle: &Handle) {
    if ui.button("get explanations").clicked() {
        http::get_explanations(ctx.clone(), handle);

        if handle.get_content_type() == Object::Tree {}
    }

    // TODO: add blob, and maybe thunk pointing to tree.
    if matches!(handle.get_content_type(), Object::Tree | Object::Tag)
        && ui.button("get contents").clicked()
    {
        match handle.get_content_type() {
            Object::Tree => http::get_tree_contents(ctx.clone(), handle),
            Object::Tag => http::get_tag_contents(ctx.clone(), handle, None),
            _ => unreachable!(),
        }
    }
    ui.end_row();
}

pub(crate) fn add_main_node(
    ctx: HttpContext,
    handle: Handle,
    graph: &Graph,
    target_input: &mut String,
    error: &str,
) -> Ports {
    add_object(
        &ctx.egui_ctx,
        "main object",
        handle.clone(),
        Pos2::new(20.0, 20.0),
        graph.forward.get(&handle),
        |ui| {
            let middle_height = Grid::new(handle.to_hex() + " properties")
                .num_columns(2)
                .show(ui, |ui| {
                    let start_y = ui.min_rect().bottom();
                    ui.label("Handle:");
                    ui.text_edit_singleline(target_input);
                    ui.end_row();
                    ui.label("Error: ");
                    ui.label(error);
                    ui.end_row();

                    (ui.min_rect().bottom() + start_y) / 2.0
                })
                .inner;

            add_fetch_buttons(ui, ctx.clone(), &handle);

            middle_height
        },
    )
}

pub(crate) fn add_node(ctx: HttpContext, handle: Handle, graph: &Graph) -> Ports {
    add_object(
        &ctx.egui_ctx,
        handle.clone(),
        handle.clone(),
        Pos2::new(20.0, 20.0),
        graph.forward.get(&handle),
        |ui| {
            let middle_height = Grid::new(handle.to_hex() + " properties")
                .num_columns(2)
                .show(ui, |ui| {
                    let start_y = ui.min_rect().bottom();
                    ui.label("Handle:");

                    let label = Label::new(handle.to_hex())
                        .truncate(true)
                        .sense(Sense::click());
                    if ui
                        .add(label)
                        .on_hover_cursor(egui::CursorIcon::Copy)
                        .clicked()
                    {
                        ui.output_mut(|o| o.copied_text = handle.to_hex())
                    };
                    ui.end_row();

                    (ui.min_rect().bottom() + start_y) / 2.0
                })
                .inner;

            add_fetch_buttons(ui, ctx.clone(), &handle);

            middle_height
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
    get_bezier(src, src_dir, dst, dst_dir, relation_type.get_color())
}
