use author::{body, envelope, pca};
use trench_core::minifloat::NUM_CORNERS;

fn res_db(r: f64) -> f64 {
    if r <= 0.0 {
        0.0
    } else {
        -20.0 * (1.0 - r).log10()
    }
}

fn corner_label(ci: usize) -> String {
    format!(
        "M{}_Q{}_Z{}",
        if ci & 1 == 0 { "0" } else { "100" },
        if ci & 2 == 0 { "0" } else { "100" },
        if ci & 4 == 0 { "0" } else { "100" }
    )
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.first().map(String::as_str) == Some("--cube") {
        let [_, square_json, out_path] = args.as_slice() else {
            eprintln!("usage: author --cube <architecture.json> <out.body>");
            std::process::exit(2);
        };
        match author::extrude::cube_from_square(std::path::Path::new(square_json)) {
            Ok(cube) => {
                let a = &cube.audit;
                println!(
                    "{}: square crown {:.1} parity {:.1} -> cube ^{:.2} trim {:+.1} dB crown {:.1} parity {:.1} surge {:+.2} · {}",
                    cube.name,
                    cube.square_audit.crown_max_db,
                    cube.square_audit.parity_db,
                    cube.bandwidth_exponent,
                    cube.gain_trim_db,
                    a.crown_max_db,
                    a.parity_db,
                    a.surge_db,
                    if cube.pass { "PASS" } else { "FAIL" }
                );
                if let Err(e) = std::fs::write(out_path, cube.packed.to_native_bytes()) {
                    eprintln!("{out_path}: {e}");
                    std::process::exit(1);
                }
                println!("wrote {out_path} (560 bytes)");
                if !cube.pass {
                    std::process::exit(1);
                }
            }
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--design") {
        let [_, design_json, out_path] = args.as_slice() else {
            eprintln!("usage: author --design <design.json> <out.body>");
            std::process::exit(2);
        };
        let alphabet_path = std::path::Path::new("recipes/alphabet.json");
        let alphabet = if alphabet_path.exists() {
            author::design::Alphabet::load(alphabet_path).unwrap_or_else(|e| {
                eprintln!("{e}");
                std::process::exit(1);
            })
        } else {
            author::design::Alphabet::empty()
        };
        match author::design::compile(std::path::Path::new(design_json), &alphabet)
            .and_then(|d| author::design::write_body(&d, std::path::Path::new(out_path)))
        {
            Ok(verdict) => println!("{verdict}\nwrote {out_path}"),
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--curves") {
        let [_, curves_json, out_path] = args.as_slice() else {
            eprintln!("usage: author --curves <curve-design.json> <out.body>");
            std::process::exit(2);
        };
        let mut progress = |corner: &str, rms: f64| {
            println!("  {corner}: fit {rms:.2} dB rms");
        };
        match author::curves::build(std::path::Path::new(curves_json), &mut progress) {
            Ok(build) => {
                let a = &build.audit;
                println!(
                    "{}: crown {:.1}..{:.1} dB · parity {:.1} · surge {:+.1} · {}",
                    build.name,
                    a.crown_min_db,
                    a.crown_max_db,
                    a.parity_db,
                    a.surge_db,
                    if a.pass() { "PASS" } else { "FAIL" }
                );
                for f in &a.failures {
                    println!("  gate: {f}");
                }
                if !a.pass() {
                    std::process::exit(1);
                }
                let (bytes, kind): (Vec<u8>, &str) = match build.packed.to_legacy_bytes() {
                    Some(b) if build.is_square => (b.to_vec(), "240-byte legacy"),
                    _ => (build.packed.to_native_bytes().to_vec(), "560-byte native"),
                };
                if let Err(e) = std::fs::write(out_path, bytes) {
                    eprintln!("{out_path}: {e}");
                    std::process::exit(1);
                }
                println!("wrote {out_path} ({kind})");
            }
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        }
        return;
    }
    if args.first().map(String::as_str) == Some("--decompile") {
        let [_, body_path, out_path] = args.as_slice() else {
            eprintln!("usage: author --decompile <in.body> <out.design.json>");
            std::process::exit(2);
        };
        let bytes = std::fs::read(body_path).unwrap_or_else(|e| {
            eprintln!("{body_path}: {e}");
            std::process::exit(1);
        });
        let packed = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes)
            .unwrap_or_else(|e| {
                eprintln!("{body_path}: {e}");
                std::process::exit(1);
            });
        let name = std::path::Path::new(body_path)
            .file_stem()
            .map(|s| s.to_string_lossy().into_owned())
            .unwrap_or_else(|| "decompiled".into());
        let doc = author::design::decompile(&packed, &name);
        std::fs::write(out_path, serde_json::to_string_pretty(&doc).unwrap()).unwrap_or_else(
            |e| {
                eprintln!("{out_path}: {e}");
                std::process::exit(1);
            },
        );
        println!("wrote {out_path}");
        return;
    }
    let [basis_dir, out_path] = args.as_slice() else {
        eprintln!(
            "usage: author <basis-dir> <out.body>  |  author --cube <architecture.json> <out.body>  |  author --design <design.json> <out.body>  |  author --decompile <in.body> <out.design.json>"
        );
        std::process::exit(2);
    };

    let files = envelope::scan(std::path::Path::new(basis_dir));
    if files.len() < 3 {
        eprintln!("{basis_dir} holds {} envelopes; a basis needs 3", files.len());
        std::process::exit(1);
    }
    let mut rows = Vec::with_capacity(files.len());
    for (_, path) in &files {
        match envelope::read_vvtf(path) {
            Ok(curve) => rows.push(envelope::on_grid(&curve)),
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        }
    }
    let basis = match pca::fit(&rows, 3) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    };
    println!(
        "basis: {} envelopes, {} components, {:.1}% variance, sigma {:?}",
        rows.len(),
        basis.components.len(),
        100.0 * basis.explained,
        basis
            .sigma
            .iter()
            .map(|s| (s * 10.0).round() / 10.0)
            .collect::<Vec<f64>>()
    );

    let curves: [Vec<f64>; NUM_CORNERS] = std::array::from_fn(|ci| {
        let scores: Vec<f64> = basis
            .sigma
            .iter()
            .enumerate()
            .map(|(axis, sigma)| if ci >> axis & 1 == 1 { *sigma } else { -sigma })
            .collect();
        basis.reconstruct(&scores)
    });

    let build = match body::build(&curves) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    };

    print!("corners:");
    for (ci, rms) in build.corner_rms_db.iter().enumerate() {
        print!(" {} {:.2}", corner_label(ci), rms);
    }
    println!(" dB rms · {} sections", build.sections_used);

    println!("lanes (M0_Q0_Z0 -> M100_Q0_Z0):");
    for (lane, (a, b)) in build.corners[0].iter().zip(&build.corners[1]).enumerate() {
        if a.pole_r <= 0.0 && a.zero_r <= 0.0 && b.pole_r <= 0.0 && b.zero_r <= 0.0 {
            continue;
        }
        let travel = if a.pole_hz > 0.0 && b.pole_hz > 0.0 {
            12.0 * (b.pole_hz / a.pole_hz).log2()
        } else {
            0.0
        };
        println!(
            "  S{}  pole {:7.0} -> {:7.0} Hz ({:+5.1} st)  res {:4.1} -> {:4.1} dB  zero {:7.0} -> {:7.0} Hz  r {:.3} -> {:.3}",
            lane + 1,
            a.pole_hz,
            b.pole_hz,
            travel,
            res_db(a.pole_r),
            res_db(b.pole_r),
            a.zero_hz,
            b.zero_hz,
            a.zero_r,
            b.zero_r
        );
    }
    let audit = &build.audit;
    println!(
        "audit: crown {:.1}..{:.1} dB · parity {:.1} dB · interior surge {:+.2} dB · stable {} · {}",
        audit.crown_min_db,
        audit.crown_max_db,
        audit.parity_db,
        audit.surge_db,
        audit.stable,
        if audit.pass() { "PASS" } else { "FAIL" }
    );
    for f in &audit.failures {
        println!("  gate: {f}");
    }

    let bytes = build.packed.to_native_bytes();
    if let Err(e) = std::fs::write(out_path, bytes) {
        eprintln!("{out_path}: {e}");
        std::process::exit(1);
    }
    println!("wrote {out_path} ({} bytes)", bytes.len());
    if !audit.pass() {
        std::process::exit(1);
    }
}
