#!/usr/bin/env python3
"""
tools/query_corpus.py
Forensic inspection tool focused on Pole-Zero Geometric Relativity.
Analyzes interval separation (Δst), radius ratios, baseline shelves, and cross-stage interactions.
"""

import sys
import os
import json
import math
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
P2K_DIR = ROOT / "recipes" / "architectures"
MORPHEUS_JSON = ROOT / "ref" / "morpheus" / "cubes_decoded.json"

NUM_POINTS = 64
FREQS = [40.0 * (16000.0 / 40.0) ** (i / (NUM_POINTS - 1)) for i in range(NUM_POINTS)]

def hz_to_st(hz, ref_hz=440.0):
    if hz <= 0:
        return 0.0
    return 12.0 * math.log2(hz / ref_hz)

def eval_section_db(p_hz, p_r, z_hz, z_r, sr, freqs):
    out = []
    for f in freqs:
        w = 2.0 * math.pi * f / sr
        wp = 2.0 * math.pi * p_hz / sr if p_hz > 0 else 0.0
        wz = 2.0 * math.pi * z_hz / sr if z_hz > 0 else 0.0
        
        cos_w = math.cos(w)
        cos_2w = math.cos(2.0 * w)
        sin_w = math.sin(w)
        sin_2w = math.sin(2.0 * w)
        
        num_re = 1.0 - 2.0 * z_r * math.cos(wz) * cos_w + (z_r * z_r) * cos_2w
        num_im = 2.0 * z_r * math.cos(wz) * sin_w - (z_r * z_r) * sin_2w
        num_mag2 = max(1e-12, num_re * num_re + num_im * num_im)
        
        den_re = 1.0 - 2.0 * p_r * math.cos(wp) * cos_w + (p_r * p_r) * cos_2w
        den_im = 2.0 * p_r * math.cos(wp) * sin_w - (p_r * p_r) * sin_2w
        den_mag2 = max(1e-12, den_re * den_re + den_im * den_im)
        
        db = 10.0 * math.log10(num_mag2 / den_mag2)
        out.append(db)
    return out

def classify_relativity(p_hz, p_r, z_hz, z_r, curve):
    """
    Classifies the geometric relationship between pole and zero in a single second-order section.
    """
    if p_r == 0.0 and z_r == 0.0:
        return "IDENTITY", "Passthrough (0 dB flat)"
    if p_r == 0.0 and z_r > 0.0:
        return "LONE_ZERO", f"Pure notch/cut at {z_hz:.0f}Hz"
    if z_r == 0.0 and p_r > 0.0:
        return "LONE_POLE", f"Pure resonant pole at {p_hz:.0f}Hz"
        
    delta_st = hz_to_st(z_hz) - hz_to_st(p_hz) if (p_hz > 0 and z_hz > 0) else 0.0
    tilt = curve[-1] - curve[0]
    pk = max(curve)
    tr = min(curve)
    
    # Check for unit circle null
    is_null = z_r >= 0.995
    null_tag = " [TRUE NULL]" if is_null else ""
    
    # 1. Spanning letters (> 24 semitones separation)
    if delta_st <= -24.0:
        return "SPANNING_SCOOP", f"Pole {p_hz:.0f}Hz vs Zero {z_hz:.0f}Hz ({delta_st:.1f}st) -> Tilt {tilt:+.1f}dB{null_tag}"
    if delta_st >= 24.0:
        return "SPANNING_LIFT", f"Pole {p_hz:.0f}Hz vs Zero {z_hz:.0f}Hz ({delta_st:+.1f}st) -> Tilt {tilt:+.1f}dB{null_tag}"
        
    # 2. Local carves (< 3.5 semitones separation)
    if abs(delta_st) <= 3.5:
        if abs(p_r - z_r) < 0.04:
            return "LOCAL_CARVE", f"Tight carve ({delta_st:+.1f}st, Zero @ {z_hz:.0f}Hz balances Pole @ {p_hz:.0f}Hz)"
        elif p_r > z_r:
            return "PEAK_CARVE", f"Pole dominates (Peak +{pk:.1f}dB, Zero notched at {delta_st:+.1f}st)"
        else:
            return "NOTCH_CARVE", f"Zero dominates (Dip {tr:.1f}dB, Pole at {delta_st:+.1f}st)"
            
    # 3. Riding zero (0.5 to 12 st above pole)
    if 0.5 < delta_st <= 12.0:
        return "RIDING_ZERO", f"Zero sits +{delta_st:.1f}st above pole (Peak +{pk:.1f}dB / Dip {tr:.1f}dB){null_tag}"
        
    # 4. Shoulder notch (-12 to -0.5 st below pole)
    if -12.0 <= delta_st < -0.5:
        return "SHOULDER_ZERO", f"Zero sits {delta_st:.1f}st below pole (Peak +{pk:.1f}dB / Dip {tr:.1f}dB){null_tag}"
        
    # 5. General Octave relationship
    return "OCTAVE_PAIR", f"Separation {delta_st:+.1f}st -> Peak +{pk:.1f}dB, Tilt {tilt:+.1f}dB{null_tag}"

def inspect_corner_relativity(name, corner_name, sections, sr=39062.5):
    composite = [0.0 for _ in FREQS]
    sec_data = []
    
    print(f"\n{'='*90}")
    print(f" POLE-ZERO RELATIVE GEOMETRY: {name} [{corner_name}]")
    print(f"{'='*90}")
    
    # Table Header
    print(f"{'Stage':<5} | {'Pole (Hz / r)':<22} | {'Zero (Hz / r)':<22} | {'Δ Interval':<11} | {'Δ Radius':<9} | {'Shelf/Tilt':<10} | {'Relativity Class'}")
    print("-" * 90)
    
    active_poles = []
    active_zeros = []
    
    for s in sections:
        slot = s.get("slot", 1)
        ph = s.get("p_hz", 0)
        pr = s.get("p_r", 0)
        zh = s.get("z_hz", 0)
        zr = s.get("z_r", 0)
        
        c = eval_section_db(ph, pr, zh, zr, sr, FREQS)
        for i in range(len(FREQS)):
            composite[i] += c[i]
            
        p_str = f"{ph:6.1f}Hz (r={pr:.4f})" if pr > 0 else "---"
        z_str = f"{zh:6.1f}Hz (r={zr:.4f})" if zr > 0 else "---"
        
        delta_st = (hz_to_st(zh) - hz_to_st(ph)) if (ph > 0 and zh > 0) else 0.0
        delta_st_str = f"{delta_st:+6.1f} st" if (ph > 0 and zh > 0) else "---"
        
        delta_r = (pr - zr) if (pr > 0 and zr > 0) else 0.0
        delta_r_str = f"{delta_r:+6.4f}" if (ph > 0 and zr > 0) else "---"
        
        tilt = c[-1] - c[0]
        tilt_str = f"{tilt:+6.1f} dB" if (ph > 0 or zr > 0) else "  0.0 dB"
        
        rel_class, rel_desc = classify_relativity(ph, pr, zh, zr, c)
        
        if pr > 0.5:
            active_poles.append((ph, pr, slot))
        if zr > 0.5:
            active_zeros.append((zh, zr, slot))
            
        print(f"S{slot:<4} | {p_str:<22} | {z_str:<22} | {delta_st_str:<11} | {delta_r_str:<9} | {tilt_str:<10} | {rel_class}")
        sec_data.append((slot, ph, pr, zh, zr, rel_class, rel_desc))
        
    print("-" * 90)
    
    # Detailed Relativity Explanations
    print("\nDETAILED PER-SECTION RELATIVE MECHANICS:")
    for slot, ph, pr, zh, zr, rel_class, rel_desc in sec_data:
        if ph > 0 or zr > 0:
            print(f"  • Stage {slot} [{rel_class}]: {rel_desc}")
            
    # Cross-Stage Relativity Analysis
    print("\nCROSS-STAGE INTERLEAVING & PROXIMITY (Inter-Section Relationships):")
    
    # Check for beating poles (< 2 st apart)
    beating_found = False
    for i in range(len(active_poles)):
        for j in range(i + 1, len(active_poles)):
            p1_hz, p1_r, s1 = active_poles[i]
            p2_hz, p2_r, s2 = active_poles[j]
            diff_st = abs(hz_to_st(p1_hz) - hz_to_st(p2_hz))
            if diff_st <= 2.0:
                print(f"  ⚡ BEATING POLE PAIR: S{s1} ({p1_hz:.1f}Hz) and S{s2} ({p2_hz:.1f}Hz) sit {diff_st:.1f} st apart! (Creates acoustic turbulence/beating)")
                beating_found = True
                
    # Check for zeros interleaving neighbor poles
    interleave_found = False
    for z_hz, z_r, z_slot in active_zeros:
        for p_hz, p_r, p_slot in active_poles:
            if z_slot != p_slot:
                diff_st = abs(hz_to_st(z_hz) - hz_to_st(p_hz))
                if diff_st <= 3.0:
                    print(f"  ✂️ INTER-STAGE CARVE: S{z_slot}'s Zero ({z_hz:.1f}Hz) carves S{p_slot}'s Pole ({p_hz:.1f}Hz) — sits {diff_st:.1f} st away.")
                    interleave_found = True
                    
    if not beating_found and not interleave_found:
        print("  • Stages operate in independent frequency corridors (no tight cross-stage collisions).")
        
    print("-" * 90)

def query_relativity(args):
    q = args.name.lower()
    
    # 1. Check P2K
    for p in sorted(P2K_DIR.glob("*.json")):
        if q in p.stem.lower():
            d = json.loads(p.read_text(encoding="utf-8"))
            name = d.get("name", p.stem)
            sr = d.get("datum_sr_hz", 39062.5)
            corners = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
            target_corners = [args.corner] if args.corner else corners
            
            for c_name in target_corners:
                sections = []
                for s in d.get("sections", []):
                    slot = s.get("slot", 1)
                    c_data = s.get("corners", {}).get(c_name, {})
                    p_val = c_data.get("pole") or {}
                    z_val = c_data.get("zero") or {}
                    sections.append({
                        "slot": slot,
                        "p_hz": p_val.get("hz", 0),
                        "p_r":  p_val.get("r", 0),
                        "z_hz": z_val.get("hz", 0),
                        "z_r":  z_val.get("r", 0),
                    })
                inspect_corner_relativity(f"P2K: {name}", c_name, sections, sr=sr)
            return

    # 2. Check Morpheus
    if MORPHEUS_JSON.exists():
        data = json.loads(MORPHEUS_JSON.read_text(encoding="utf-8"))
        for c in data.get("cubes", []):
            if q in c.get("name", "").lower() or q in f"{c.get('index', 0)}":
                name = c.get("name")
                cube_corners = c.get("corners", [])
                c_indices = [int(args.corner)] if args.corner and args.corner.isdigit() else range(len(cube_corners))
                for c_idx in c_indices:
                    if c_idx >= len(cube_corners):
                        continue
                    c_data = cube_corners[c_idx]
                    sections = []
                    for s_idx, s in enumerate(c_data.get("sections", [])):
                        p_val = s.get("pole") or {}
                        z_val = s.get("zero") or {}
                        sections.append({
                            "slot": s_idx + 1,
                            "p_hz": p_val.get("hz", 0),
                            "p_r":  p_val.get("r", 0),
                            "z_hz": z_val.get("hz", 0),
                            "z_r":  z_val.get("r", 0),
                        })
                    inspect_corner_relativity(f"Cube #{c['index']}: {name}", f"Corner {c_idx}", sections, sr=39062.5)
                return

    print(f"No preset or cube found matching '{args.name}'.")

def main():
    parser = argparse.ArgumentParser(description="Inspect Pole-Zero Relativity in E-mu filter bodies.")
    parser.add_argument("name", help="Filter name or index (e.g. TalkingHedz, EarBender, 143, Swirly)")
    parser.add_argument("--corner", "-c", help="Specific corner to inspect (e.g. M0_Q0, M100_Q0, 0, 1)")
    
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)
        
    args = parser.parse_args()
    query_relativity(args)

if __name__ == "__main__":
    main()
