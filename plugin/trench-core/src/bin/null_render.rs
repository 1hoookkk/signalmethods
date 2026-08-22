// Render a dry wav through a body at a fixed Morph/Q, on the real engine path.
// Purpose: null a TRENCH render against an X3 render of the same body.
use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;

const BLOCK: usize = 128;

fn read_wav_f32(path: &str) -> (u32, u16, Vec<f32>) {
    let d = std::fs::read(path).expect("read wav");
    assert_eq!(&d[0..4], b"RIFF", "not a RIFF file");
    let (mut rate, mut ch, mut bits, mut fmt_tag) = (0u32, 0u16, 0u16, 0u16);
    let mut samples: Vec<f32> = Vec::new();
    let mut i = 12usize;
    while i + 8 <= d.len() {
        let id = &d[i..i + 4];
        let sz = u32::from_le_bytes([d[i + 4], d[i + 5], d[i + 6], d[i + 7]]) as usize;
        let body = &d[i + 8..(i + 8 + sz).min(d.len())];
        if id == b"fmt " {
            fmt_tag = u16::from_le_bytes([body[0], body[1]]);
            ch = u16::from_le_bytes([body[2], body[3]]);
            rate = u32::from_le_bytes([body[4], body[5], body[6], body[7]]);
            bits = u16::from_le_bytes([body[14], body[15]]);
        } else if id == b"data" {
            match bits {
                32 => {
                    samples = body
                        .chunks_exact(4)
                        .map(|c| {
                            let v = u32::from_le_bytes([c[0], c[1], c[2], c[3]]);
                            if fmt_tag == 3 || fmt_tag == 0xFFFE {
                                f32::from_bits(v)
                            } else {
                                (v as i32) as f32 / 2147483648.0
                            }
                        })
                        .collect()
                }
                24 => {
                    samples = body
                        .chunks_exact(3)
                        .map(|c| {
                            let v =
                                ((c[2] as i32) << 24 | (c[1] as i32) << 16 | (c[0] as i32) << 8)
                                    >> 8;
                            v as f32 / 8388608.0
                        })
                        .collect()
                }
                16 => {
                    samples = body
                        .chunks_exact(2)
                        .map(|c| i16::from_le_bytes([c[0], c[1]]) as f32 / 32768.0)
                        .collect()
                }
                _ => panic!("unsupported bit depth {bits}"),
            }
        }
        i += 8 + sz + (sz & 1);
    }
    (rate, ch, samples)
}

fn write_wav_f32(path: &str, rate: u32, ch: u16, data: &[f32]) {
    let bytes_len = (data.len() * 4) as u32;
    let mut w: Vec<u8> = Vec::with_capacity(44 + data.len() * 4);
    w.extend_from_slice(b"RIFF");
    w.extend_from_slice(&(36 + bytes_len).to_le_bytes());
    w.extend_from_slice(b"WAVEfmt ");
    w.extend_from_slice(&16u32.to_le_bytes());
    w.extend_from_slice(&3u16.to_le_bytes()); // IEEE float
    w.extend_from_slice(&ch.to_le_bytes());
    w.extend_from_slice(&rate.to_le_bytes());
    w.extend_from_slice(&(rate * ch as u32 * 4).to_le_bytes());
    w.extend_from_slice(&(ch * 4).to_le_bytes());
    w.extend_from_slice(&32u16.to_le_bytes());
    w.extend_from_slice(b"data");
    w.extend_from_slice(&bytes_len.to_le_bytes());
    for v in data {
        w.extend_from_slice(&v.to_le_bytes());
    }
    std::fs::write(path, w).expect("write wav");
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 6 {
        eprintln!(
            "usage: null-render <body.body240> <in.wav> <out.wav> <morph 0..1> <q 0..1> [rate]"
        );
        std::process::exit(2);
    }
    let body = std::fs::read(&args[1]).expect("read body");
    let (in_rate, in_ch, interleaved) = read_wav_f32(&args[2]);
    let morph: f64 = args[4].parse().expect("morph 0..1");
    let q: f64 = args[5].parse().expect("q 0..1");
    let rate: f64 = args
        .get(6)
        .map(|s| s.parse().expect("rate"))
        .unwrap_or(in_rate as f64);

    let frames = interleaved.len() / in_ch as usize;
    let mut l: Vec<f32> = Vec::with_capacity(frames);
    let mut r: Vec<f32> = Vec::with_capacity(frames);
    for f in 0..frames {
        let a = interleaved[f * in_ch as usize];
        let b = if in_ch > 1 {
            interleaved[f * in_ch as usize + 1]
        } else {
            a
        };
        l.push(a);
        r.push(b);
    }

    let mut eng = FilterEngine::new();
    eng.prepare(rate);
    let cart = Cartridge::from_body_bytes("null", &body, 1.0).expect("cartridge");
    eng.load_cartridge(cart);

    let mut out: Vec<f32> = Vec::with_capacity(frames * 2);
    let mut off = 0usize;
    while off < frames {
        let n = BLOCK.min(frames - off);
        let mut bl = l[off..off + n].to_vec();
        let mut br = r[off..off + n].to_vec();
        eng.process_block(&mut bl, &mut br, morph, q);
        for i in 0..n {
            out.push(bl[i]);
            out.push(br[i]);
        }
        off += n;
    }
    write_wav_f32(&args[3], rate as u32, 2, &out);
    println!(
        "rendered {} frames @ {} Hz  morph={} q={}  -> {}",
        frames, rate, morph, q, args[3]
    );
}
