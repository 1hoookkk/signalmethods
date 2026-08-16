import glob, json, os

p2k_files = sorted(glob.glob('recipes/architectures/P2k_*.json'))
print(f"Found {len(p2k_files)} ground-truth P2K architecture recipes in recipes/architectures/")

for pf in p2k_files[:10]:
    with open(pf, 'r') as f:
        data = json.load(f)
    name = data.get('name')
    sr = data.get('datum_sr_hz', 39062.5)
    sections = data.get('sections', [])
    print(f"\n--- Preset #{data.get('index')}: {name} (datum_sr: {sr} Hz, {len(sections)} stages) ---")
    for s in sections:
        slot = s.get('slot')
        corners = s.get('corners', {})
        c0 = corners.get('M0_Q0', {})
        p = c0.get('pole', {})
        z = c0.get('zero', {})
        scale = c0.get('scale', 1.0)
        p_hz = p.get('hz', 0.0)
        p_r = p.get('r', 0.0)
        z_hz = z.get('hz', 0.0)
        z_r = z.get('r', 0.0)
        print(f"  Stage S{slot} M0_Q0: Pole={p_hz:7.1f} Hz (r={p_r:.4f}), Zero={z_hz:7.1f} Hz (r={z_r:.4f}), Scale={scale:.4f}")
