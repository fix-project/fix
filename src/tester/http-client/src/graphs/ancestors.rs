use std::collections::HashMap;

use eframe::epaint::{util::FloatOrd, CubicBezierShape};
use egui::{
    plot::{
        items::{
            values::{ClosestElem, PlotGeometry},
            PlotConfig, PlotItem,
        },
        LabelFormatter, PlotBounds, PlotPoint, PlotTransform,
    },
    Color32, Pos2, Shape, Stroke, Ui,
};

use crate::{
    handle::{Handle, Operation, Task},
    plot::Element,
};

/// An element and all of its ancestors. This graph is append only.
/// The relative locations of Ancestors should never change.
/// Note any single Handle may only appear once in the graph.
#[derive(Clone, Debug)]
pub(super) struct AncestorGraph {
    inner: [Ancestor; 1],
    /// Used to reference to id's
    lineages: HashMap<Handle, (OrderingIndex, Lineage)>,
    /// Defined ordering of Handles. Used to reference from id's
    ordering: Vec<Handle>,
}

#[derive(Clone, Copy, PartialEq, Debug, Eq)]
pub struct OrderingIndex(usize);

#[derive(Clone, Debug)]
/// Index positions into the tree of Ancestors.
struct Lineage(Vec<usize>);

/// An element and all of its ancestors
#[derive(Clone)]
pub struct Ancestor {
    // An Element for rendering
    content: Element,
    /// Parents that render above this Ancestor's contained Element
    parents: Vec<Ancestor>,
    /// Children Handles that are pointed to.
    children: Vec<(OrderingIndex, Operation)>,
}

/// The direction an outgoing arrow should go.
enum Arrow {
    Down,
    Right,
}

impl PlotItem for AncestorGraph {
    fn shapes(&self, ui: &mut Ui, transform: &PlotTransform, shapes: &mut Vec<Shape>) {
        for (index, el) in self.iter().enumerate() {
            el.add_shapes(
                transform,
                shapes,
                self.get_draw_parameters(index),
                false,
                ui.visuals().widgets.active.fg_stroke.color,
            );
        }
        self.add_arrows(transform, shapes);
    }

    fn initialize(&mut self, _x_range: std::ops::RangeInclusive<f64>) {}

    fn name(&self) -> &str {
        "Ancestor Tree"
    }

    fn color(&self) -> Color32 {
        Color32::TRANSPARENT
    }

    fn highlight(&mut self) {}

    fn highlighted(&self) -> bool {
        false
    }

    fn geometry(&self) -> PlotGeometry<'_> {
        PlotGeometry::Rects
    }

    /// Search for the closest element in the graph based on squared distance to bounds.
    fn find_closest(&self, point: Pos2, transform: &PlotTransform) -> Option<ClosestElem> {
        self.iter()
            .enumerate()
            .map(|(index, el)| {
                let bounds = el.bounds(self.get_draw_parameters(index));
                let rect = transform.rect_from_values(&bounds.min().into(), &bounds.max().into());
                ClosestElem {
                    index,
                    dist_sq: rect.distance_sq_to_pos(point),
                }
            })
            .min_by_key(|e| e.dist_sq.ord())
    }

    fn on_hover(
        &self,
        elem: ClosestElem,
        shapes: &mut Vec<Shape>,
        _: &mut Vec<egui::plot::Cursor>,
        plot: &PlotConfig<'_>,
        _: &LabelFormatter,
    ) {
        let entry = self.iter().nth(elem.index);
        let Some(entry) = entry else { return };
        entry.add_highlight(plot.transform, self.get_draw_parameters(elem.index), shapes);
    }

    fn bounds(&self) -> PlotBounds {
        let mut bounds = PlotBounds::NOTHING;
        for (index, el) in self.iter().enumerate() {
            bounds.merge(&el.bounds(self.get_draw_parameters(index)));
        }
        bounds
    }
}

impl AncestorGraph {
    pub fn new(element: Element) -> Self {
        let ordering = vec![element.get_handle().clone()];
        let mut lineages = HashMap::new();
        lineages.insert(
            element.get_handle().clone(),
            (OrderingIndex(0), Lineage(vec![0])),
        );
        Self {
            inner: [Ancestor::new(element, vec![])],
            ordering,
            lineages,
        }
    }

    /// Handle a click that is near to a ClosestElem. May send an http request
    /// that is specified by the `request` parameter.
    pub fn handle_nearby_click(
        &self,
        ui: &Ui,
        coords: PlotPoint,
        closest_elem: ClosestElem,
        request: impl FnOnce(usize, &Handle),
    ) {
        let Some(elem) = self.iter().nth(closest_elem.index) else {
            log::error!("Handling a click near to an element whose index no longer exists");
            return;
        };

        let params = self.get_draw_parameters(closest_elem.index);
        let [min_x, min_y] = elem.bounds(params).min();
        let [max_x, max_y] = elem.bounds(params).max();
        let p = coords;
        let elem_contains_p = min_x <= p.x && p.x <= max_x && min_y <= p.y && p.y <= max_y;
        if elem_contains_p {
            ui.output_mut(|o| o.copied_text = elem.get_handle().to_hex());
            log::info!("Requesting parents");
            // TODO use index instead of hashmap
            request(0, elem.get_handle());
        };
    }

    fn get_from_lineage<'a>(root_slice: &'a [Ancestor], lineage: &Lineage) -> &'a Ancestor {
        let (last_index, rest) = lineage
            .0
            .as_slice()
            .split_last()
            .expect("lineage should never be empty");
        let mut generation = root_slice;
        for index in rest {
            generation = generation[*index].parents.as_slice();
        }
        &generation[*last_index]
    }

    fn get_mut_from_lineage<'a>(
        root_slice: &'a mut [Ancestor],
        lineage: &Lineage,
    ) -> &'a mut Ancestor {
        let (last_index, rest) = lineage
            .0
            .as_slice()
            .split_last()
            .expect("lineage should never be empty");
        let mut generation = root_slice;
        for index in rest {
            generation = generation[*index].parents.as_mut_slice();
        }
        &mut generation[*last_index]
    }

    fn iter(&self) -> impl Iterator<Item = &Element> {
        self.ordering.iter().map(|handle| {
            let lineage = &self
                .lineages
                .get(handle)
                .expect("handle from ordering does not exist in ancestry graph locations")
                .1;
            &Self::get_from_lineage(&self.inner, lineage).content
        })
    }

    fn find(&mut self, handle: &Handle) -> Option<&mut Ancestor> {
        let lineage = self.lineages.get(handle)?.clone();
        Some(Self::get_mut_from_lineage(&mut self.inner, &lineage.1))
    }

    fn get_draw_parameters(&self, index: usize) -> (PlotPoint, f64) {
        const Y_SCALE: f64 = 0.5;

        let lineage = &self.lineages.get(&self.ordering[index]).unwrap().1 .0;

        // Set the position to be (0, -pos) so that the first vertical offset puts
        // the main object at (0, 0).
        let mut scale = 1.0;
        let mut pos = [0.0, -scale * Y_SCALE];
        let mut current_generation = self.inner.as_slice();
        for lineage_index in lineage {
            // Scale y for this generation
            scale /= current_generation.len() as f64;
            // Increase y (by half relative to the x)
            pos[1] += scale * Y_SCALE;
            // Offset x
            // |   0   |   1   |   2   |
            // |  0  |  1  |  2  |  3  |
            let step_size = scale;
            let x_step_offset_to_left_edge =
                *lineage_index as f64 - (current_generation.len() as f64) * 0.5;
            let x_step_offset_to_center = x_step_offset_to_left_edge + 0.5;
            pos[0] += step_size * x_step_offset_to_center;

            current_generation = current_generation[*lineage_index].parents.as_slice();
        }

        (PlotPoint::new(pos[0], pos[1]), scale)
    }

    pub fn merge_new_parents(&mut self, ui: &egui::Ui, handle: Handle, incoming_parents: &[Task]) {
        let (child_index, child_lineage) = self
            .lineages
            .get(&handle)
            .cloned()
            .expect("the target child for merging new parents must exist");
        for parent in incoming_parents {
            // If the parent already exists, add this handle as a child.
            if let Some(ancestor) = self.find(&parent.handle) {
                let op = parent.operation;
                ancestor.add_child(&child_index, op);
            } else {
                // Otherwise, add the parent above the child.
                // Must update the ancestor lineages map, the ancestors ordering
                // list, and the target child's parent list.
                let target_list =
                    &mut Self::get_mut_from_lineage(&mut self.inner, &child_lineage).parents;
                let lineage_index = target_list.len();
                let ancestor_index = OrderingIndex(self.ordering.len());
                self.lineages.insert(
                    parent.handle.clone(),
                    (ancestor_index, {
                        let mut new_lineage = child_lineage.clone();
                        new_lineage.0.push(lineage_index);
                        new_lineage
                    }),
                );
                self.ordering.push(parent.handle.clone());
                target_list.push(Ancestor::new(
                    Element::new(ui, parent.handle.clone()),
                    vec![(child_index, parent.operation)],
                ));
            }
        }
    }

    fn add_arrows(&self, transform: &PlotTransform, shapes: &mut Vec<Shape>) {
        // For every handle in this graph,
        for handle in self.ordering.iter() {
            // Obtain its lineage
            let (index, lineage) = self
                .lineages
                .get(handle)
                .expect("handle in ordering is not in lineages");
            let container = Self::get_from_lineage(&self.inner, lineage);
            let o_draw_params = self.get_draw_parameters(index.0);
            // and determine its bounding box.
            let o_bbox = container.content.bounds(o_draw_params);
            // Then, for every child of this handle,
            for child in &container.children {
                // set the origin_point to be the right middle or center bottom of the handle's bounding box
                // if the origin maps to itself or to another index, respectively
                let (o_point, direction) = if *index == child.0 {
                    ([o_bbox.max()[0], o_bbox.center().y].into(), Arrow::Right)
                } else {
                    ([o_bbox.center().x, o_bbox.min()[1]].into(), Arrow::Down)
                };
                // and get the target's bounding box.
                let t_draw_params = self.get_draw_parameters(child.0 .0);
                let t_bbox = Self::get_from_lineage(
                    &self.inner,
                    &self.lineages[&self.ordering[child.0 .0]].1,
                )
                .content
                .bounds(t_draw_params);
                // Set the target_point to be the center top of the child box.
                let t_point = PlotPoint::new(t_bbox.center().x, t_bbox.max()[1]);
                // Draw an arrow from the center bottom of the origin to the center top of the target.
                Self::add_arrow(
                    transform,
                    shapes,
                    (o_point, o_draw_params.1 / 5.0, direction),
                    (t_point, t_draw_params.1 / 5.0),
                    child.1.get_color(),
                )
            }
        }
    }

    /// Draws an arrow from the origin to the target using a cubic bezier curve
    /// that weighs the control points according to the scales at each end.
    fn add_arrow(
        transform: &PlotTransform,
        shapes: &mut Vec<Shape>,
        origin: (PlotPoint, f64, Arrow),
        target: (PlotPoint, f64),
        color: Color32,
    ) {
        let arrow_scale = f64::min(origin.1, target.1);
        let tip_scale = arrow_scale / 40.0;
        let stroke = Stroke::new(
            (arrow_scale * transform.dpos_dvalue_x() / 100.0) as f32,
            color,
        );
        let origin_control = match origin.2 {
            Arrow::Down => PlotPoint::new(origin.0.x, origin.0.y - origin.1),
            Arrow::Right => PlotPoint::new(origin.0.x + origin.1, origin.0.y),
        };
        let target_control = PlotPoint::new(target.0.x, target.0.y + target.1);
        let head_start = PlotPoint::new(target.0.x - tip_scale, target.0.y + tip_scale);
        let head_end = PlotPoint::new(target.0.x + tip_scale, target.0.y + tip_scale);
        let origin = transform.position_from_point(&origin.0);
        let origin_control = transform.position_from_point(&origin_control);
        let target_control = transform.position_from_point(&target_control);
        let target = transform.position_from_point(&target.0);
        let arrow_body = CubicBezierShape::from_points_stroke(
            [origin, origin_control, target_control, target],
            false,
            Color32::TRANSPARENT,
            stroke,
        );
        let arrow_head = Shape::line(
            vec![
                transform.position_from_point(&head_start),
                target,
                transform.position_from_point(&head_end),
            ],
            stroke,
        );
        shapes.push(arrow_body.into());
        shapes.push(arrow_head);
    }
}

impl std::fmt::Debug for Ancestor {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!(
            "{{content: {}, parents: [",
            self.content.get_text()
        ))?;
        for parent in &self.parents {
            f.write_fmt(format_args!("{:?}, ", parent))?;
        }
        f.write_str("], children: [")?;
        for child in &self.children {
            f.write_fmt(format_args!("{:?}, ", child))?;
        }
        f.write_str("]}")
    }
}

impl Ancestor {
    fn new(content: Element, children: Vec<(OrderingIndex, Operation)>) -> Self {
        Ancestor {
            content,
            parents: vec![],
            children,
        }
    }

    fn add_child(&mut self, incoming_child: &OrderingIndex, operation: Operation) {
        // Linear scan, performance irrelevant for small lists of children.
        if self
            .children
            .iter()
            .all(|(index, op)| (index != incoming_child) && (*op != operation))
        {
            self.children.push((*incoming_child, operation));
        }
    }
}
