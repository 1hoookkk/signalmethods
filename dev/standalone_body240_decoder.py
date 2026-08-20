"""
STANDALONE BODY240 DECODER & MULTISET VALIDATOR
Pure Python (zero dependencies). Reads raw 240-byte E-mu P2K binary bodies.
"""

import struct, math, sys, os

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0

def decode_u16_minifloat(word):
    """
    E-mu minifloat decoding:
    16-bit word: 4-bit exponent (bits 12..15), 12-bit mantissa (bits 0..11).
    Word is biased by +1.
    """
    u = int(word) + 1
    if u >= 65536:
        return 1.0
    if u <= 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))

def pair_to_roots(d_mag, d_rsq, sr=SR_39K):
    """
    Converts Rossum transformed coordinates (d_mag, d_rsq) into physical (hz, r).
    p = 4*d_mag + d_rsq - 2
    q = 1 - d_rsq
    Polynomial: z^2 + p*z + q = 0
    """
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if abs(p) < 1e-12 and abs(q) < 1e-12:
        return {"type": "off", "hz": 0.0, "r": 0.0}
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = math.sqrt(max(0.0, q))
        cw = max(-1.0, min(1.0, -p / (2.0 * r))) if r > 0 else 0.0
        return {"type": "conj", "hz": (math.acos(cw) / TAU) * sr, "r": r}
    else:
        s = math.sqrt(disc)
        return {"type": "real", "a": (-p + s) / 2.0, "b": (-p - s) / 2.0}

def parse_body240(filepath, sr=SR_39K):
    """
    Parses a 240-byte body file:
    6 stages x 4 corners x (pole_mag, pole_rsq, zero_mag, zero_rsq) + 4 corner gains
    """
    with open(filepath, "rb") as f:
        data = f.read()
    if len(data) != 240:
        raise ValueError(f"Expected 240 bytes, got {len(data)}")
    
    words = struct.unpack("<120H", data)
    corners = [{"name": f"C{i}", "sections": [], "gain_raw": 0} for i in range(4)]
    
    w_idx = 0
    for stage_idx in range(6):
        for corner_idx in range(4):
            pm_w = words[w_idx]
            pr_w = words[w_idx + 1]
            zm_w = words[w_idx + 2]
            zr_w = words[w_idx + 3]
            w_idx += 4
            
            pm = decode_u16_minifloat(pm_w)
            pr = decode_u16_minifloat(pr_w)
            zm = decode_u16_minifloat(zm_w)
            zr = decode_u16_minifloat(zr_w)
            
            p_roots = pair_to_roots(pm, pr, sr)
            z_roots = pair_to_roots(zm, zr, sr)
            
            sec = {
                "stage": stage_idx + 1,
                "raw_words": [pm_w, pr_w, zm_w, zr_w],
                "minifloats": [pm, pr, zm, zr],
                "pole": p_roots,
                "zero": z_roots
            }
            if len(corners[corner_idx]["sections"]) <= stage_idx:
                corners[corner_idx]["sections"].append(sec)
                
    return corners

def eval_cascade_db(corner, hz_list, sr=SR_39K):
    """
    Evaluates exact complex H(z) in dB.
    """
    results = []
    for hz in hz_list:
        w = (TAU * hz) / sr
        cw = math.cos(w)
        sw = math.sin(w)
        c2 = cw * cw - sw * sw
        s2 = 2.0 * sw * cw
        
        power = 1.0
        for s in corner["sections"]:
            p = s["pole"]
            z = s["zero"]
            
            # Numerator (Zero)
            if z["type"] == "conj":
                th_z = (TAU * z["hz"]) / sr
                b1 = -2.0 * z["r"] * math.cos(th_z)
                b2 = z["r"] * z["r"]
                num_re = 1.0 + b1 * cw + b2 * c2
                num_im = b1 * sw + b2 * s2
            elif z["type"] == "real":
                num_re = (1.0 - z["a"] * cw) * (1.0 - z["b"] * cw) - (z["a"] * sw) * (z["b"] * sw)
                num_im = -(z["a"] + z["b"]) * sw + z["a"] * z["b"] * s2
            else:
                num_re, num_im = 1.0, 0.0
            num_pwr = num_re * num_re + num_im * num_im
            
            # Denominator (Pole)
            if p["type"] == "conj":
                th_p = (TAU * p["hz"]) / sr
                a1 = -2.0 * p["r"] * math.cos(th_p)
                a2 = p["r"] * p["r"]
                den_re = 1.0 + a1 * cw + a2 * c2
                den_im = a1 * sw + a2 * s2
            elif p["type"] == "real":
                den_re = (1.0 - p["a"] * cw) * (1.0 - p["b"] * cw) - (p["a"] * sw) * (p["b"] * sw)
                den_im = -(p["a"] + p["b"]) * sw + p["a"] * p["b"] * s2
            else:
                den_re, den_im = 1.0, 0.0
            den_pwr = max(den_re * den_re + den_im * den_im, 1e-18)
            
            power *= (num_pwr / den_pwr)
            
        results.append(10.0 * math.log10(max(power, 1e-12)))
    return results

if __name__ == "__main__":
    print("=== STANDALONE P2K BODY240 VERIFIER ===")
    candidates_1 = ["P2k_010_ooh_to_eee.body240", "plots/inspector/P2k_010_ooh_to_eee.body240"]
    candidates_2 = ["P2k_021_ubu_orator.body240", "plots/inspector/P2k_021_ubu_orator.body240"]
    
    p1 = [p for p in candidates_1 if os.path.exists(p)]
    p2 = [p for p in candidates_2 if os.path.exists(p)]
    
    if p1 and p2:
        p1, p2 = p1[0], p2[0]
        c1 = parse_body240(p1)
        c2 = parse_body240(p2)
        
        print(f"Loaded {p1} and {p2}")
        print("\n--- P2k_010 Corner 0 Raw Words & Roots ---")
        for i, s in enumerate(c1[0]["sections"]):
            print(f"  S{i+1}: raw={s['raw_words']} -> Pole={s['pole']['hz']:.1f}Hz (r={s['pole']['r']:.4f}) | Zero={s['zero']['hz']:.1f}Hz (r={s['zero']['r']:.4f})")
            
        print("\n--- P2k_021 Corner 0 Raw Words & Roots ---")
        for i, s in enumerate(c2[0]["sections"]):
            print(f"  S{i+1}: raw={s['raw_words']} -> Pole={s['pole']['hz']:.1f}Hz (r={s['pole']['r']:.4f}) | Zero={s['zero']['hz']:.1f}Hz (r={s['zero']['r']:.4f})")
            
        # Frequency response comparison across 100 log frequencies
        hz_test = [40.0 * ((16000.0/40.0)**(i/99.0)) for i in range(100)]
        h1 = eval_cascade_db(c1[0], hz_test)
        h2 = eval_cascade_db(c2[0], hz_test)
        
        max_diff = max(abs(a - b) for a, b in zip(h1, h2))
        print(f"\nFrequency Response Max Abs Diff across 40-16000 Hz: {max_diff:.6f} dB")
        if max_diff < 1e-4:
            print(">> VERIFIED: The two presets produce mathematically IDENTICAL composite transfer functions at Corner 0!")
