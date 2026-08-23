import glob, math, os, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

SR = 44100.0
F = 55 * (2.0) ** (np.arange(120) / 12.0)
F = F[F < 18000]
w = 2 * np.pi * F / SR
def pair(hz, r): return 1 - 2 * r * math.cos(2 * math.pi * hz / SR) * np.exp(-1j * w) + r * r * np.exp(-2j * w)

def roots(corner):
    out = []
    for g in corner:
        for kind in ('pole', 'zero'):
            rt = getattr(g, kind)
            if rt.kind == 'conjugate': out.append((kind, rt.hz, rt.r))
            elif rt.kind == 'real': out.append((kind + 'R', rt.root_a, rt.root_b))
            else: out.append((kind + '0', 0, 0))
    return out

def lerp_response(a, b, m):
    h = np.ones_like(F, dtype=complex)
    for ra, rb in zip(a, b):
        if ra[0] == 'pole' and rb[0] == 'pole' or ra[0] == 'zero' and rb[0] == 'zero':
            hz = math.exp((1 - m) * math.log(max(ra[1], 20)) + m * math.log(max(rb[1], 20)))
            l1 = lambda r: math.log(max(1 - r, 1e-7))
            r = 1 - math.exp((1 - m) * l1(ra[2]) + m * l1(rb[2]))
            h = h / pair(hz, r) if ra[0] == 'pole' else h * pair(hz, r)
    return 20 * np.log10(np.abs(h) + 1e-9)

N = 24
print("perceptual step size along Morph (dB spectral change per 1/24 step, ERB-ish log grid):")
for path in ['P2k_013_talking_hedz', 'P2k_006_bassbox_303', 'P2k_010_ooh_to_eee', 'P2k_008_dead_ringer']:
    c = dl.decode_p2k_body(open(f'ref/presets/{path}.bin', 'rb').read(), SR)
    a, b = roots(c[0]), roots(c[1])
    resp = [lerp_response(a, b, i / N) for i in range(N + 1)]
    steps = [float(np.sqrt(np.mean((resp[i + 1] - resp[i]) ** 2))) for i in range(N)]
    s = np.array(steps)
    print(f"  {path[8:]:16s} min {s.min():4.1f}  max {s.max():4.1f}  ratio x{s.max()/max(s.min(),1e-6):4.1f}   profile " + " ".join(f"{x:.0f}" for x in s))
