use eframe::epaint::util::FloatOrd;
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

/// Note that there may be duplicate `Progress` if, say, a tree contains two of
/// the same canonical Object.
#[derive(Clone)]
pub(super) struct ProgressGraph {
    inner: Progress,
    ordering: Vec<DependeeStack>,
}

/// Counterpart of `Lineage` for ancestors.
/// Used to index into nested `Progress`
#[derive(Clone)]
struct DependeeStack(Vec<usize>);

/// The basic unit of the ProgressGraph.
/// Should be rendered as
/// ```text
/// task.handle
///      |
///      | <--- dependees[0]
///      |
///      | <--- dependees[1]
///      |          .
///      |          .
///      |          .
///      v
///    result
/// ```
/// where the arrow is colored corresponding with task.operation and each
/// dependee has its own subtree.
#[derive(Clone)]
struct Progress {
    task: (Element, Operation),
    result: Option<Element>,
    dependees: Vec<Progress>,
}

#[derive(Clone, Copy)]
struct DrawParams {
    task: (PlotPoint, f64),
    result: (PlotPoint, f64),
}

enum Arrow {
    Left,
    Down,
}

impl PlotItem for ProgressGraph {
    fn shapes(&self, ui: &mut Ui, transform: &PlotTransform, shapes: &mut Vec<Shape>) {
        for (i, stack) in self.ordering.iter().enumerate() {
            let progress = self.get_from_stack(stack);
            let draw_params = self.get_draw_parameters(stack);
            let bounds = progress.task.0.bounds(draw_params.task);
            let color = ui.visuals().widgets.active.fg_stroke.color;
            // Draw task's element.
            progress
                .task
                .0
                .add_shapes(transform, shapes, draw_params.task, false, color);
            // Draw result if exists.
            if let Some(result) = &progress.result {
                result.add_shapes(transform, shapes, draw_params.result, false, color);
            }
            // Draw an arrow to the left.
            // The top level task is not the dependee of others, so only draw an
            // to the left for not top level Progress's.
            if i != 0 {
                let mut draw_params = draw_params;
                // Take the left middle of the bounds.
                draw_params.task.0 = [bounds.min()[0], bounds.center().y].into();
                draw_params.result.0 =
                    [bounds.center().x - draw_params.task.1, bounds.center().y].into();
                Self::add_arrow(transform, shapes, draw_params, Arrow::Left, color);
            }
            // Draw operation arrow
            // Take the bottom center of the task and top center of result if it exists.
            let mut draw_params = draw_params;
            draw_params.task.0 = [bounds.center().x, bounds.min()[1]].into();
            draw_params.result.0 = if let Some(r) = &progress.result {
                let b = r.bounds(draw_params.result);
                [b.center().x, b.max()[1]]
            } else {
                [bounds.center().x, bounds.center().y - draw_params.task.1]
            }
            .into();
            Self::add_arrow(
                transform,
                shapes,
                draw_params,
                Arrow::Down,
                progress.task.1.get_color(),
            );
        }
    }

    fn initialize(&mut self, _x_range: std::ops::RangeInclusive<f64>) {}

    fn name(&self) -> &str {
        "Dependency Tree"
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
        self.ordering
            .iter()
            .enumerate()
            .map(|(index, stack)| {
                let progress = self.get_from_stack(stack);
                let params = self.get_draw_parameters(stack);
                let get_distance = |el: &Element, params: (PlotPoint, f64)| -> f64 {
                    let bounds = el.bounds(params);
                    transform
                        .rect_from_values(&bounds.min().into(), &bounds.max().into())
                        .distance_sq_to_pos(point) as f64
                };
                let task_dist = get_distance(&progress.task.0, params.task);
                let result_dist = progress
                    .result
                    .as_ref()
                    .map(|el| get_distance(el, params.result));
                ClosestElem {
                    index,
                    dist_sq: (task_dist.min(result_dist.unwrap_or(f64::MAX))) as f32,
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
        let stack = &self.ordering[elem.index];
        let progress = self.get_from_stack(stack);
        let params = self.get_draw_parameters(stack);
        progress
            .task
            .0
            .add_highlight(plot.transform, params.task, shapes);
        if let Some(result) = &progress.result {
            result.add_highlight(plot.transform, params.result, shapes);
        }
    }

    fn bounds(&self) -> PlotBounds {
        PlotBounds::from_min_max([-0.2, -1.2], [1.2, 0.2])
    }
}

impl ProgressGraph {
    pub fn new(element: Element, operation: Operation) -> Self {
        let ordering = vec![DependeeStack(vec![])];
        Self {
            inner: Progress {
                task: (element, operation),
                result: None,
                dependees: vec![],
            },
            ordering,
        }
    }

    /// Handle a click that is near to a ClosestElem. May send an http request
    /// that is specified by the `request` parameter.
    pub fn handle_nearby_click(
        &self,
        ui: &Ui,
        coords: PlotPoint,
        closest_elem: ClosestElem,
        request: impl FnOnce(usize, Handle, Operation),
    ) {
        let stack = &self.ordering[closest_elem.index];
        let params = self.get_draw_parameters(stack);
        let progress = self.get_from_stack(stack);

        let contains = |elem: &Element, params: (PlotPoint, f64)| -> bool {
            let [min_x, min_y] = elem.bounds(params).min();
            let [max_x, max_y] = elem.bounds(params).max();
            let p = coords;
            min_x <= p.x && p.x <= max_x && min_y <= p.y && p.y <= max_y
        };

        if contains(&progress.task.0, params.task) {
            ui.output_mut(|o| o.copied_text = progress.task.0.get_handle().to_hex());
            request(
                closest_elem.index,
                progress.task.0.get_handle().clone(),
                progress.task.1,
            )
        } else if progress
            .result
            .as_ref()
            .is_some_and(|r| contains(r, params.result))
        {
            ui.output_mut(|o| {
                o.copied_text = progress.result.clone().unwrap().get_handle().to_hex()
            });
        }
    }

    pub fn set_child(&mut self, ui: &Ui, index: usize, child: Handle) {
        let ordering = self.ordering[index].clone();
        self.get_mut_from_stack(&ordering).result = Some(Element::new(ui, child));
    }

    pub fn merge_dependees(&mut self, ui: &Ui, index: usize, dependees: Vec<Task>) {
        let ordering = self.ordering[index].clone();

        let original_list = &mut self.get_mut_from_stack(&ordering).dependees;
        let mut new_stacks = vec![];
        for dependee in dependees {
            if !original_list.iter().any(|p| {
                p.task.0.get_handle() == &dependee.handle && p.task.1 == dependee.operation
            }) {
                let new_index = original_list.len();
                original_list.push(Progress {
                    task: (Element::new(ui, dependee.handle), dependee.operation),
                    result: None,
                    dependees: vec![],
                });
                let mut new_stack = ordering.clone();
                new_stack.0.push(new_index);
                new_stacks.push(new_stack);
            }
        }
        self.ordering.append(&mut new_stacks);
        log::error!("{:?}", self.ordering.len());
    }

    fn get_from_stack(&self, stack: &DependeeStack) -> &Progress {
        let mut current_progress = &self.inner;
        for index in stack.0.iter() {
            current_progress = &current_progress.dependees[*index];
        }
        current_progress
    }

    fn get_mut_from_stack(&mut self, stack: &DependeeStack) -> &mut Progress {
        let mut current_progress = &mut self.inner;
        for index in stack.0.iter() {
            current_progress = &mut current_progress.dependees[*index];
        }
        current_progress
    }

    /// Returns the top left (least x, max y) point of the bounding box for
    // the task location and the bottom left for the result location.
    fn get_draw_parameters(&self, stack: &DependeeStack) -> DrawParams {
        let mut scale = 1.0;
        let mut location = [0.0, 0.0];
        let mut current_progress = &self.inner;
        for index in stack.0.iter() {
            scale /= 2.0 * current_progress.dependees.len() as f64;
            let initial_vertical_offset = scale / 2.0;
            let index_vertical_offset = *index as f64 * scale * 2.0;
            let horizontal_offset = scale;
            location[0] += horizontal_offset;
            location[1] -= initial_vertical_offset + index_vertical_offset;
            current_progress = &current_progress.dependees[*index];
        }
        let result = {
            let mut result_location = location;
            result_location[1] -= scale;
            (PlotPoint::from(result_location), scale)
        };
        DrawParams {
            task: (PlotPoint::from(location), scale),
            result,
        }
    }

    fn add_arrow(
        transform: &PlotTransform,
        shapes: &mut Vec<Shape>,
        draw_params: DrawParams,
        direction: Arrow,
        color: Color32,
    ) {
        let scale = draw_params.task.1;
        let origin = draw_params.task.0;
        let target = draw_params.result.0;
        let tip_scale = scale / 40.0;
        let stroke = Stroke::new((scale * transform.dpos_dvalue_x() / 100.0) as f32, color);
        let (head_start, head_end) = match direction {
            Arrow::Left => (
                PlotPoint::from([target.x + tip_scale, target.y + tip_scale]),
                PlotPoint::from([target.x + tip_scale, target.y - tip_scale]),
            ),
            Arrow::Down => (
                PlotPoint::from([target.x - tip_scale, target.y + tip_scale]),
                PlotPoint::from([target.x + tip_scale, target.y + tip_scale]),
            ),
        };
        shapes.push(Shape::line_segment(
            [
                transform.position_from_point(&origin),
                transform.position_from_point(&target),
            ],
            stroke,
        ));
        shapes.push(Shape::line(
            vec![
                transform.position_from_point(&head_start),
                transform.position_from_point(&target),
                transform.position_from_point(&head_end),
            ],
            stroke,
        ));
    }
}
