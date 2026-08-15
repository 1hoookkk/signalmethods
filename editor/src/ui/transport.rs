use eframe::egui::{Id, Painter, Pos2, Rect, Sense, Shape, Stroke, Ui, Vec2};

use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme};

pub fn draw(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    at: Pos2,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) -> Rect {
    let rect = Rect::from_min_size(at, Vec2::new(34.0, 24.0));
    let inner = if session.audition.playing {
        let i = paint::sunken(painter, rect);
        painter.rect_filled(i, 0.0, theme::CHROME_DK);
        i
    } else {
        paint::raised(painter, rect)
    };
    let c = inner.center();
    if session.audition.playing {
        for dx in [-4.0, 1.5] {
            painter.rect_filled(
                Rect::from_min_size(Pos2::new(c.x + dx, c.y - 5.0), Vec2::new(2.6, 10.0)),
                0.0,
                theme::HOT,
            );
        }
    } else {
        painter.add(Shape::convex_polygon(
            vec![
                Pos2::new(c.x - 3.5, c.y - 5.5),
                Pos2::new(c.x + 4.5, c.y),
                Pos2::new(c.x - 3.5, c.y + 5.5),
            ],
            theme::NOW_INK,
            Stroke::NONE,
        ));
    }
    let resp = ui.interact(rect, Id::new("transport"), Sense::click());
    if resp.hovered() {
        *legend = if session.audition.playing {
            "L pause".into()
        } else {
            "L play".into()
        };
    }
    if resp.clicked() {
        cmds.push(Command::TogglePlay);
    }
    rect
}
