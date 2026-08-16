mod brief;
mod field;
mod fit;
mod http;
mod json;
mod library;
mod parity;
mod store;
mod target;
mod ws;

use std::net::{TcpListener, TcpStream};
use std::sync::Arc;

use store::Store;

fn main() {
    let store = Arc::new(Store::new().unwrap_or_else(|e| {
        eprintln!("{e}");
        std::process::exit(1);
    }));
    let addr = std::env::args().nth(1).unwrap_or_else(|| "127.0.0.1:8787".into());
    let listener = TcpListener::bind(&addr).unwrap_or_else(|e| {
        eprintln!("bind {addr}: {e}");
        std::process::exit(1);
    });
    println!("trench workstation at http://{addr}  (root: {})", store.root.display());
    for stream in listener.incoming() {
        let Ok(stream) = stream else { continue };
        let store = Arc::clone(&store);
        std::thread::spawn(move || handle(stream, &store));
    }
}

fn handle(mut stream: TcpStream, store: &Store) {
    let req = match http::read_request(&mut stream) {
        Ok(r) => r,
        Err(e) => return http::error(&mut stream, 400, &e),
    };
    if req.path == "/ws" {
        return words_socket(&mut stream, &req);
    }
    match (req.method.as_str(), req.path.as_str()) {
        ("GET", "/api/library") => http::json(&mut stream, 200, &library::library(store)),
        ("GET", "/api/brief") => match brief::brief(store, &req.query) {
            Ok(v) => http::json(&mut stream, 200, &v),
            Err(e) => http::error(&mut stream, 400, &e),
        },
        ("POST", "/api/target") => api(&mut stream, &req.body, |v| target::target(store, v)),
        ("POST", "/api/skeleton") => api(&mut stream, &req.body, fit::skeleton),
        ("POST", "/api/response") => api(&mut stream, &req.body, fit::response),
        ("POST", "/api/fit") => api(&mut stream, &req.body, fit::fit),
        ("POST", "/api/fit_stream") => fit_stream(&mut stream, &req.body),
        ("POST", "/api/audit") => api(&mut stream, &req.body, field::audit),
        ("POST", "/api/corners") => api(&mut stream, &req.body, field::corners_words),
        ("POST", "/api/write_body") => api(&mut stream, &req.body, |v| field::write_body(store, v)),
        ("POST", "/api/write_frame") => api(&mut stream, &req.body, |v| field::write_frame(store, v)),
        ("POST", "/api/frame") => api(&mut stream, &req.body, |v| field::load_frame(store, v)),
        ("GET", "/api/parity_vectors") => match parity_endpoint(store, &req.query) {
            Ok(v) => http::json(&mut stream, 200, &v),
            Err(e) => http::error(&mut stream, 400, &e),
        },
        ("GET", path) => {
            let rel = if path == "/" { "index.html" } else { path.trim_start_matches('/') };
            if rel.contains("..") || rel.contains(':') {
                return http::error(&mut stream, 400, "bad path");
            }
            http::file(&mut stream, &store.root.join("workstation").join(rel));
        }
        _ => http::error(&mut stream, 404, "no such endpoint"),
    }
}

fn words_socket(stream: &mut TcpStream, req: &http::Request) {
    let Some(key) = http::header(&req.headers, "sec-websocket-key") else {
        return http::error(stream, 400, "not a websocket handshake");
    };
    if !ws::accept(stream, key) {
        return;
    }
    let mut frames = ws::Frames::new(stream, req.buffered.clone());
    while let Some((opcode, payload)) = frames.read() {
        match opcode {
            0x8 => break,
            0x9 => {
                if !frames.write(0xA, &payload) {
                    break;
                }
            }
            0x2 => {
                let Some(reply) = field::words_frame(&payload, fit::SR) else {
                    break;
                };
                if !frames.write(0x2, &reply) {
                    break;
                }
            }
            _ => {}
        }
    }
}

fn fit_stream(stream: &mut TcpStream, body: &[u8]) {
    let request = match json::parse(body) {
        Ok(v) => v,
        Err(e) => return http::error(stream, 400, &e),
    };
    let (tx, rx) = std::sync::mpsc::channel();
    let worker = std::thread::spawn(move || {
        let mut emit = |v: serde_json::Value| {
            let _ = tx.send(v);
        };
        let done = match fit::fit_watched(&request, &mut emit) {
            Ok(v) => serde_json::json!({ "result": v }),
            Err(e) => serde_json::json!({ "error": e }),
        };
        let _ = tx.send(done);
    });
    http::chunked_start(stream);
    let mut alive = true;
    for message in rx {
        if alive {
            alive = http::chunk_line(stream, &message);
        }
    }
    http::chunked_end(stream);
    let _ = worker.join();
}

fn parity_endpoint(store: &Store, query: &str) -> Result<serde_json::Value, String> {
    let n = http::query_param(query, "n").and_then(|v| v.parse().ok()).unwrap_or(64);
    let id = http::query_param(query, "id").unwrap_or("../ref/presets/P2k_013_talking_hedz.bin");
    let legacy = id.starts_with("../ref/");
    let path = if legacy {
        store.root.join(id.trim_start_matches("../"))
    } else {
        store.resolve(id)?
    };
    let sr = if legacy { author::extrude::AUTHORING_SR } else { fit::SR };
    let bytes = std::fs::read(&path).map_err(|e| e.to_string())?;
    let packed = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes)?;
    Ok(parity::vectors(&packed, n, sr))
}

fn api(
    stream: &mut TcpStream,
    body: &[u8],
    handler: impl Fn(&serde_json::Value) -> Result<serde_json::Value, String>,
) {
    match json::parse(body).and_then(|v| handler(&v)) {
        Ok(v) => http::json(stream, 200, &v),
        Err(e) => http::error(stream, 400, &e),
    }
}
