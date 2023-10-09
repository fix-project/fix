use eframe::epaint::{ClippedShape, Primitive, RectShape, TextShape};
use egui::{
    plot::{PlotBounds, PlotPoint, PlotTransform},
    Color32, Mesh, Pos2, Rect, RichText, Shape, Stroke, TextStyle, Ui, WidgetText,
};

use crate::handle::Handle;

#[derive(Clone)]
pub(crate) struct Element {
    content: Handle,
    mesh: Mesh,
    mesh_bounds: Rect,
}

impl PartialEq for Element {
    fn eq(&self, other: &Self) -> bool {
        self.content == other.content
    }
}

impl Element {
    const TEXT_RENDER_SCALE: f64 = 30.0;
    const RECT_EXTENSION: f64 = 0.02;
    /// The number of pixels just a full rendered handle takes.
    /// Used to scale the text.
    const TEXT_PIXEL_SCALE: f64 = 40.0;

    pub(crate) fn new(ui: &Ui, content: Handle) -> Self {
        let rich_text = RichText::new(content.to_string())
            .size(Self::TEXT_RENDER_SCALE as f32)
            .monospace()
            .color(ui.visuals().widgets.active.fg_stroke.color);
        let galley = WidgetText::RichText(rich_text).into_galley(
            ui,
            Some(false),
            f64::INFINITY as f32,
            TextStyle::Monospace,
        );
        if let Primitive::Mesh(mut mesh) = ui
            .ctx()
            .tessellate(vec![ClippedShape(
                Rect::EVERYTHING,
                TextShape::new(Pos2::ZERO, galley.galley).into(),
            )])
            .pop()
            .unwrap()
            .primitive
        {
            let mid_point =
                (mesh.calc_bounds().min.to_vec2() + mesh.calc_bounds().max.to_vec2()) / 2.0;
            mesh.translate(-mid_point);
            mesh.vertices.iter_mut().for_each(|v| {
                v.pos = Pos2::new(
                    (v.pos.x as f64 / Self::TEXT_RENDER_SCALE / Self::TEXT_PIXEL_SCALE) as f32,
                    (v.pos.y as f64 / Self::TEXT_RENDER_SCALE / Self::TEXT_PIXEL_SCALE) as f32,
                );
            });
            Self {
                content,
                mesh_bounds: mesh.calc_bounds().expand(Self::RECT_EXTENSION as f32),
                mesh,
            }
        } else {
            panic!("Tessellated text should be a mesh")
        }
    }

    fn graph_pos_to_screen_pos(
        position: PlotPoint,
        transform: &PlotTransform,
        zoom: f64,
        center: PlotPoint,
    ) -> Pos2 {
        let screen_center = transform.position_from_point(&center);
        Pos2::new(
            (position.x * transform.dpos_dvalue_x() * zoom + screen_center.x as f64) as f32,
            (-position.y * transform.dpos_dvalue_y() * zoom + screen_center.y as f64) as f32,
        )
    }

    pub(crate) fn add_shapes(
        &self,
        transform: &PlotTransform,
        shapes: &mut Vec<Shape>,
        (center, zoom): (PlotPoint, f64),
        highlight: bool,
        fg_stroke_color: Color32,
    ) {
        let transform = |pos: PlotPoint| -> Pos2 {
            Self::graph_pos_to_screen_pos(pos, transform, zoom, center)
        };

        let mut mesh = self.mesh.clone();
        mesh.vertices.iter_mut().for_each(|v| {
            v.pos = transform(PlotPoint::new(v.pos.x, v.pos.y));
        });

        let mut mesh_bounds = self.mesh_bounds;
        mesh_bounds.min = transform(PlotPoint::new(mesh_bounds.min.x, mesh_bounds.min.y));
        mesh_bounds.max = transform(PlotPoint::new(mesh_bounds.max.x, mesh_bounds.max.y));

        shapes.push(Shape::Mesh(mesh.clone()));
        shapes.push(Shape::rect_stroke(
            mesh_bounds,
            1.0,
            Stroke::new(2.0, fg_stroke_color),
        ));
        if highlight {
            shapes.push(Shape::rect_filled(mesh_bounds, 1.0, fg_stroke_color));
        }
    }

    pub(crate) fn add_highlight(
        &self,
        transform: &PlotTransform,
        (center, zoom): (PlotPoint, f64),
        shapes: &mut Vec<Shape>,
    ) {
        let transform = transform;
        let scale_transform = |pos: Pos2| -> Pos2 {
            Pos2::new(
                (pos.x as f64 * transform.dpos_dvalue_x() * zoom) as f32,
                (-pos.y as f64 * transform.dpos_dvalue_y() * zoom) as f32,
            )
        };
        let translation = transform.position_from_point(&center).to_vec2();
        let mut mesh_bounds = self.mesh_bounds;
        mesh_bounds.min = scale_transform(mesh_bounds.min);
        mesh_bounds.max = scale_transform(mesh_bounds.max);
        mesh_bounds = mesh_bounds.translate(translation);

        shapes.push(RectShape::filled(mesh_bounds, 1.0, Color32::BLUE.gamma_multiply(0.2)).into())
    }

    pub(crate) fn bounds(&self, (center, zoom): (PlotPoint, f64)) -> PlotBounds {
        let rect = self.mesh_bounds;

        assert!(rect.center() == Pos2::ZERO);
        let left = rect.min.x as f64 * zoom + center.x;
        let right = rect.max.x as f64 * zoom + center.x;
        let bottom = rect.min.y as f64 * zoom + center.y;
        let top = rect.max.y as f64 * zoom + center.y;

        // Reverse the y axis because of rect vs plot coordinates.
        PlotBounds::from_min_max([left, bottom], [right, top])
    }

    pub(crate) fn get_text(&self) -> String {
        self.content.to_string()
    }

    pub(crate) fn get_handle(&self) -> &Handle {
        &self.content
    }
}
