//! Laws and authoring grammar.
//!
//! A law names a quantity and the span it must sit inside. The Station measures
//! the quantity and reports it against the span the operator wrote. It never
//! proposes a span, and it never adjusts the filter to satisfy one.
//!
//! Grammar is separate from law: grammar declares the vocabulary the Station
//! works in — what the axes are called, what the lanes are called, what the
//! response view spans — while law declares what must hold. Both are edited as
//! text and both are consequential: changing either changes what the Station
//! does on the next frame, not just what a file says.

use serde::{Deserialize, Serialize};

use super::analysis::{self, CascadeResponse};
use super::project::Project;
use crate::ui::editor::Located;

/// Which scope a law is measured over.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Scope {
    /// Measured at one authored frame.
    Frame,
    /// Measured across the whole object, sweeping the axes.
    Body,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Law {
    #[serde(default = "default_scope")]
    pub on: Scope,
    pub q: String,
    pub min: f64,
    pub max: f64,
    #[serde(default)]
    pub why: String,
}

fn default_scope() -> Scope {
    Scope::Frame
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize, Default)]
pub struct LawFile {
    #[serde(default)]
    pub laws: Vec<Law>,
}

/// The authoring grammar: the vocabulary and the working span.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Grammar {
    /// Names for the declared axes, in axis order. Extra entries are ignored;
    /// missing ones fall back to the axis's own name.
    #[serde(default)]
    pub axis_names: Vec<String>,
    /// Names for lanes, in lane order.
    #[serde(default)]
    pub lane_names: Vec<String>,
    /// Names for frames, in corner order. A frame is easier to steer toward by
    /// what it sounds like than by its address, so the operator may name it.
    #[serde(default)]
    pub frame_names: Vec<String>,
    /// The response view's frequency span.
    #[serde(default = "default_lo")]
    pub display_lo_hz: f64,
    #[serde(default = "default_hi")]
    pub display_hi_hz: f64,
    /// Radius above which a pole is called out as close to the circle. A
    /// reporting threshold, not a limit the encoder enforces.
    #[serde(default = "default_watch")]
    pub pole_radius_watch: f64,
}

fn default_lo() -> f64 {
    20.0
}
fn default_hi() -> f64 {
    20_000.0
}
fn default_watch() -> f64 {
    0.995
}

impl Default for Grammar {
    fn default() -> Self {
        Self {
            axis_names: Vec::new(),
            lane_names: Vec::new(),
            frame_names: Vec::new(),
            display_lo_hz: default_lo(),
            display_hi_hz: default_hi(),
            pole_radius_watch: default_watch(),
        }
    }
}

impl Grammar {
    pub fn axis_name(&self, i: usize, fallback: &str) -> String {
        self.axis_names
            .get(i)
            .cloned()
            .unwrap_or_else(|| fallback.to_string())
    }

    pub fn lane_name(&self, i: usize, fallback: &str) -> String {
        self.lane_names
            .get(i)
            .cloned()
            .unwrap_or_else(|| fallback.to_string())
    }

    /// A frame's name when one is declared, otherwise its address.
    pub fn frame_name(&self, i: usize, fallback: &str) -> String {
        self.frame_names
            .get(i)
            .filter(|s| !s.trim().is_empty())
            .cloned()
            .unwrap_or_else(|| fallback.to_string())
    }

    /// Grammar errors that are the grammar's own fault, not the project's.
    pub fn check(&self) -> Vec<String> {
        let mut out = Vec::new();
        if !self.display_lo_hz.is_finite() || self.display_lo_hz <= 0.0 {
            out.push("display_lo_hz must be a positive frequency".into());
        }
        if self.display_hi_hz <= self.display_lo_hz {
            out.push("display_hi_hz must be above display_lo_hz".into());
        }
        if !(0.0..=1.0).contains(&self.pole_radius_watch) {
            out.push("pole_radius_watch must lie in 0..1".into());
        }
        out
    }
}

/// Parses JSON and reports failures where they happened.
pub fn parse_laws(text: &str) -> Result<LawFile, Located> {
    let f: LawFile = serde_json::from_str(text).map_err(located)?;
    Ok(f)
}

pub fn parse_grammar(text: &str) -> Result<Grammar, Located> {
    let g: Grammar = serde_json::from_str(text).map_err(located)?;
    Ok(g)
}

fn located(e: serde_json::Error) -> Located {
    Located {
        line: e.line().max(1),
        column: e.column(),
        message: e.to_string(),
        fatal: true,
    }
}

/// Laws that name a quantity the Station cannot measure are reported at their
/// own line, so the operator is told which word was not understood.
pub fn unknown_quantities(text: &str, file: &LawFile) -> Vec<Located> {
    let mut out = Vec::new();
    for law in &file.laws {
        if KNOWN_QUANTITIES.contains(&law.q.as_str()) {
            if law.min > law.max {
                out.push(locate_token(
                    text,
                    &law.q,
                    format!("min {} is above max {}", law.min, law.max),
                    true,
                ));
            }
            continue;
        }
        out.push(locate_token(
            text,
            &law.q,
            format!("no measurable quantity is called \"{}\"", law.q),
            true,
        ));
    }
    out
}

fn locate_token(text: &str, token: &str, message: String, fatal: bool) -> Located {
    for (i, l) in text.lines().enumerate() {
        if let Some(c) = l.find(token) {
            return Located {
                line: i + 1,
                column: c + 1,
                message,
                fatal,
            };
        }
    }
    Located {
        line: 1,
        column: 1,
        message,
        fatal,
    }
}

/// Every quantity a law may name. Adding one here makes it immediately
/// available to the laws file and to the panel.
pub const KNOWN_QUANTITIES: &[&str] = &[
    "order",
    "crown_db",
    "dip_db",
    "tilt_db",
    "peaks",
    "gap_oct",
    "scale_spread_db",
    "lane_peak_db",
    "cumulative_peak_db",
    "pole_r_max",
    "dc_db",
    "meet_st",
    "meet_at",
];

pub struct Reading {
    pub law: Law,
    pub value: f64,
    pub where_: String,
    pub ok: bool,
}

fn median(v: &mut [f64]) -> f64 {
    if v.is_empty() {
        return 0.0;
    }
    v.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
    v[v.len() / 2]
}

fn count_peaks(db: &[f64], prominence: f64) -> usize {
    let w = 12usize;
    (2..db.len().saturating_sub(2))
        .filter(|&i| {
            if !(db[i] > db[i - 1] && db[i] >= db[i + 1]) {
                return false;
            }
            let lo = db[i.saturating_sub(w)..i]
                .iter()
                .cloned()
                .fold(f64::MAX, f64::min);
            let hi = db[i..(i + w).min(db.len())]
                .iter()
                .cloned()
                .fold(f64::MAX, f64::min);
            db[i] - lo.min(hi) >= prominence
        })
        .count()
}

/// Measures one frame-scope quantity from an already-computed response.
pub fn measure_frame(r: &CascadeResponse, q: &str) -> Option<f64> {
    let band: Vec<f64> = r
        .grid
        .iter()
        .zip(&r.total)
        .filter(|(f, _)| **f > 60.0 && **f < 16_000.0)
        .map(|(_, d)| *d)
        .collect();
    if band.is_empty() {
        return None;
    }
    let med = median(&mut band.clone());
    let mean_in = |lo: f64, hi: f64| -> f64 {
        let v: Vec<f64> = r
            .grid
            .iter()
            .zip(&r.total)
            .filter(|(f, _)| **f > lo && **f < hi)
            .map(|(_, d)| *d)
            .collect();
        if v.is_empty() {
            0.0
        } else {
            v.iter().sum::<f64>() / v.len() as f64
        }
    };

    Some(match q {
        // Two poles per lane carrying a conjugate pair.
        "order" => {
            r.lane_metrics
                .iter()
                .filter(|m| m.pole_r.map(|x| x > 0.0).unwrap_or(false))
                .count() as f64
                * 2.0
        }
        "crown_db" => band.iter().cloned().fold(f64::MIN, f64::max) - med,
        "dip_db" => med - band.iter().cloned().fold(f64::MAX, f64::min),
        "tilt_db" => mean_in(60.0, 300.0) - mean_in(4_000.0, 12_000.0),
        "peaks" => count_peaks(&r.total, 4.0) as f64,
        "gap_oct" => {
            let mut hz: Vec<f64> = r.lane_metrics.iter().filter_map(|m| m.pole_hz).collect();
            hz.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
            if hz.len() < 2 {
                return None;
            }
            hz.windows(2)
                .map(|w| (w[1] / w[0]).log2())
                .fold(f64::MAX, f64::min)
        }
        "scale_spread_db" => {
            let s: Vec<f64> = r
                .lane_metrics
                .iter()
                .filter(|m| !m.is_identity)
                .map(|m| 20.0 * m.scale.max(1e-9).log10())
                .collect();
            if s.is_empty() {
                return None;
            }
            s.iter().cloned().fold(f64::MIN, f64::max) - s.iter().cloned().fold(f64::MAX, f64::min)
        }
        "lane_peak_db" => r
            .lane_metrics
            .iter()
            .map(|m| m.peak_db)
            .fold(f64::MIN, f64::max),
        "cumulative_peak_db" => r.worst_intermediate().map(|(_, v)| v)?,
        "pole_r_max" => r
            .lane_metrics
            .iter()
            .filter_map(|m| m.pole_r)
            .fold(f64::MIN, f64::max),
        "dc_db" => r.lane_metrics.iter().map(|m| m.dc_db).sum(),
        _ => return None,
    })
}

/// Body-scope quantities sweep the declared axes rather than reading one frame.
/// The sweep walks axis 0 with the other axes held at zero, which is the axis
/// the operator is morphing along; nothing here assumes three axes.
pub fn measure_body(p: &Project, q: &str) -> Option<(f64, String)> {
    match q {
        "meet_st" | "meet_at" => {
            let steps = 64;
            let mut best: Option<(f64, f64, usize, usize)> = None;
            for i in 0..=steps {
                let t = i as f32 / steps as f32;
                let mut coords = vec![0.0f32; p.topology.axis_count()];
                if coords.is_empty() {
                    return None;
                }
                coords[0] = t;
                let lanes = p.cascade_at(&coords)?;
                let hz: Vec<Option<f64>> = lanes
                    .iter()
                    .map(|l| match l.geometry(p.sample_rate()).pole {
                        trench_core::stage_law::RootPair::Conjugate { hz, .. } if hz > 20.0 => {
                            Some(hz)
                        }
                        _ => None,
                    })
                    .collect();
                for a in 0..hz.len() {
                    for b in (a + 1)..hz.len() {
                        if let (Some(x), Some(y)) = (hz[a], hz[b]) {
                            let d = (12.0 * (y / x).log2()).abs();
                            if best.map(|z| d < z.0).unwrap_or(true) {
                                best = Some((d, t as f64, a, b));
                            }
                        }
                    }
                }
            }
            let (d, at, a, b) = best?;
            let note = format!(
                "{} and {} at {} {:.0}",
                super::lane::LaneId(a as u32),
                super::lane::LaneId(b as u32),
                p.topology
                    .axes
                    .first()
                    .map(|x| x.name.clone())
                    .unwrap_or_default(),
                at * 100.0
            );
            Some(if q == "meet_st" {
                (d, note)
            } else {
                (at, note)
            })
        }
        _ => None,
    }
}

/// Reads every law against one selected frame and the whole object.
pub fn read_all(p: &Project, laws: &[Law], frame: usize) -> Vec<Reading> {
    let response = p
        .frames
        .get(frame)
        .map(|f| analysis::analyse(&f.values, p.sample_rate()));
    laws.iter()
        .filter_map(|l| {
            let (value, where_) = match l.on {
                Scope::Body => measure_body(p, &l.q)?,
                Scope::Frame => {
                    let r = response.as_ref()?;
                    let v = measure_frame(r, &l.q)?;
                    (v, p.frames.get(frame)?.label.clone())
                }
            };
            Some(Reading {
                law: l.clone(),
                value,
                where_,
                ok: value >= l.min && value <= l.max,
            })
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::topology::Topology;

    fn factory() -> Project {
        let raw = std::fs::read("../ref/presets/P2k_013_talking_hedz.bin").unwrap();
        Project::from_packed("hedz", &raw, 44_100.0).unwrap()
    }

    #[test]
    fn a_malformed_law_file_is_located_at_its_line() {
        let text = "{\n  \"laws\": [\n    { \"q\": \"order\", \"min\": 1 \"max\": 2 }\n  ]\n}";
        let e = parse_laws(text).unwrap_err();
        assert_eq!(e.line, 3, "error reported at line {} of {text:?}", e.line);
        assert!(e.fatal);
    }

    #[test]
    fn a_law_naming_an_unmeasurable_quantity_is_refused_at_its_line() {
        let text = "{\n  \"laws\": [\n    { \"q\": \"loudness\", \"min\": 0, \"max\": 1 }\n  ]\n}";
        let f = parse_laws(text).unwrap();
        let errs = unknown_quantities(text, &f);
        assert_eq!(errs.len(), 1);
        assert_eq!(errs[0].line, 3);
        assert!(errs[0].message.contains("loudness"), "{}", errs[0].message);
    }

    #[test]
    fn an_inverted_span_is_refused() {
        let text = "{\n  \"laws\": [\n    { \"q\": \"crown_db\", \"min\": 9, \"max\": 2 }\n  ]\n}";
        let f = parse_laws(text).unwrap();
        let errs = unknown_quantities(text, &f);
        assert_eq!(errs.len(), 1);
        assert!(errs[0].message.contains("above max"), "{}", errs[0].message);
    }

    #[test]
    fn every_known_quantity_is_measurable_on_a_real_body() {
        let p = factory();
        let r = analysis::analyse(&p.frames[0].values, p.sample_rate());
        for q in KNOWN_QUANTITIES {
            let got = measure_frame(&r, q).is_some() || measure_body(&p, q).is_some();
            assert!(got, "quantity {q} is declared known but measures nothing");
        }
    }

    #[test]
    fn laws_read_against_a_real_body_and_report_pass_or_fail() {
        let p = factory();
        let laws = vec![
            Law {
                on: Scope::Frame,
                q: "order".into(),
                min: 0.0,
                max: 14.0,
                why: String::new(),
            },
            Law {
                on: Scope::Frame,
                q: "crown_db".into(),
                min: 1e9,
                max: 2e9,
                why: String::new(),
            },
        ];
        let rd = read_all(&p, &laws, 0);
        assert_eq!(rd.len(), 2);
        assert!(rd[0].ok, "order {} outside 0..14", rd[0].value);
        assert!(!rd[1].ok, "an impossible span must not hold");
    }

    #[test]
    fn grammar_defaults_are_usable_and_bad_grammar_is_reported() {
        let g = Grammar::default();
        assert!(g.check().is_empty());
        let bad: Grammar = serde_json::from_str(
            r#"{"display_lo_hz": 0, "display_hi_hz": -5, "pole_radius_watch": 4}"#,
        )
        .unwrap();
        assert_eq!(bad.check().len(), 3);
    }

    #[test]
    fn grammar_renames_axes_and_lanes_without_touching_the_project() {
        let g: Grammar =
            serde_json::from_str(r#"{"axis_names":["SWEEP"],"lane_names":["AIR","BODY"]}"#)
                .unwrap();
        assert_eq!(g.axis_name(0, "M"), "SWEEP");
        assert_eq!(g.axis_name(1, "Q"), "Q");
        assert_eq!(g.lane_name(1, "L2"), "BODY");
        assert_eq!(g.lane_name(5, "L6"), "L6");
    }

    #[test]
    fn body_scope_measurement_works_at_a_non_three_axis_topology() {
        let p = Project::blank(
            "flat",
            Topology::new(vec![crate::model::topology::Axis::new("a", "A")]),
            3,
        );
        // A pass-through object has no conjugate poles, so there is nothing to
        // meet — and that must be reported as no reading, not as zero.
        assert!(measure_body(&p, "meet_st").is_none());
    }
}
