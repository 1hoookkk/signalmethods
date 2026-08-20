use egui::{pos2, Rect};

pub const MAIN_DB_LO: f64 = -24.0;
pub const MAIN_DB_HI: f64 = 12.0;
pub const ERROR_DB_SPAN: f64 = 6.0;
pub const CROWN_DB: f64 = 36.0;
pub const AXIS_H: f32 = 16.0;
pub const HZ_TICKS: [f64; 9] = [40.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 16000.0];

#[derive(Clone, Copy)]
pub struct DbAxis {
    top: f32,
    height: f32,
    lo: f64,
    hi: f64,
}

impl DbAxis {
    pub fn new(rect: Rect, lo: f64, hi: f64) -> Self {
        Self {
            top: rect.top(),
            height: rect.height().max(1.0),
            lo,
            hi,
        }
    }

    pub fn main(rect: Rect) -> Self {
        Self::new(rect, MAIN_DB_LO, MAIN_DB_HI)
    }

    pub fn error(rect: Rect) -> Self {
        Self::new(rect, -ERROR_DB_SPAN, ERROR_DB_SPAN)
    }

    pub fn y_of(&self, db: f64) -> f32 {
        let t = (db - self.lo) / (self.hi - self.lo);
        self.top + self.height * (1.0 - t as f32)
    }

    pub fn db_of(&self, y: f32) -> f64 {
        let t = ((y - self.top) / self.height).clamp(0.0, 1.0) as f64;
        self.lo + (1.0 - t) * (self.hi - self.lo)
    }

    pub fn lo(&self) -> f64 {
        self.lo
    }

    pub fn hi(&self) -> f64 {
        self.hi
    }
}

pub struct Split {
    pub main: Rect,
    pub axis: Rect,
    pub error: Rect,
}

impl Split {
    pub fn of(rect: Rect) -> Self {
        Self::with_error(rect, true)
    }

    pub fn with_error(rect: Rect, show_error: bool) -> Self {
        let error_h = if show_error {
            (rect.height() * 0.265).round().clamp(150.0, 320.0)
        } else {
            0.0
        };
        let main_h = (rect.height() - AXIS_H - error_h).max(1.0);
        let main = Rect::from_min_max(
            rect.min,
            pos2(rect.right(), rect.top() + main_h),
        );
        let axis = Rect::from_min_max(
            pos2(rect.left(), main.bottom()),
            pos2(rect.right(), main.bottom() + AXIS_H),
        );
        let error = Rect::from_min_max(pos2(rect.left(), axis.bottom()), rect.max);
        Self { main, axis, error }
    }
}

pub fn hz_label(hz: f64) -> String {
    if hz >= 1000.0 {
        format!("{}k", (hz / 1000.0).round() as i64)
    } else {
        format!("{}", hz.round() as i64)
    }
}
