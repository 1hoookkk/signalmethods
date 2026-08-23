import math, struct, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

SR = 44100.0
def roots_of(body, ci):
    st = dl.decode_p2k_body(open(f'ref/presets/P2k_{body}.bin', 'rb').read(), SR)[ci]
    out = []
    for g in st:
        for kind, rt in (('p', g.pole), ('z', g.zero)):
            if rt.kind == 'conjugate': out.append((math.log(max(rt.hz, 20.0)), math.log(max(1 - rt.r, 1e-7))))
            elif rt.kind == 'real': out.append((math.log(20.0), math.log(max(1 - abs(rt.root_a), 1e-7))))
            else: out.append((math.log(20.0), 0.0))
    return np.array(out)

A = roots_of('010_ooh_to_eee', 0); B = roots_of('010_ooh_to_eee', 1); C = roots_of('013_talking_hedz', 0)

def coeffs(enc):
    out = []
    for i in range(0, len(enc), 2):
        (lp, lq), (lz, lqz) = enc[i], enc[i + 1]
        def biq(lh, l1r, pole):
            hz = math.exp(lh); r = 1 - math.exp(l1r); r = min(r, 0.999999)
            t = 2 * math.pi * hz / SR
            return (-2 * r * math.cos(t), r * r)
        a1, a2 = biq(lp, lq, True); b1, b2 = biq(lz, lqz, False)
        dc = (1 + a1 + a2) / max(1 + b1 + b2, 1e-9)
        out.append((dc, b1, b2, a1, a2))
    return out

def render(path_fn, seconds=6.0):
    n = int(SR * seconds); out = np.zeros(n)
    z = [[0.0, 0.0] for _ in range(6)]
    phase = 0.0; step = 55.0 / SR
    block = 64
    for off in range(0, n, block):
        m = off / n
        cs = coeffs(path_fn(m))
        ln = min(block, n - off)
        for i in range(ln):
            phase += step
            if phase >= 1.0: phase -= 1.0
            x = 0.25 * (2 * phase - 1)
            for s, (dc, b1, b2, a1, a2) in enumerate(cs):
                w = x - a1 * z[s][0] - a2 * z[s][1]
                x = dc * (w + b1 * z[s][0] + b2 * z[s][1])
                z[s][1] = z[s][0]; z[s][0] = w
            out[off + i] = x
    peak = np.max(np.abs(out)) or 1
    return (out / peak * 0.5)

def wav(path, x):
    d = (np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(d)) + b'WAVEfmt ' +
                struct.pack('<IHHIIHH', 16, 1, 1, int(SR), int(SR) * 2, 2, 16) + b'data' + struct.pack('<I', len(d)) + d)

lin = lambda m: (1 - m) * A + m * B
bez = lambda m: (1 - m) ** 2 * A + 2 * m * (1 - m) * C + m * m * B
pw  = lambda m: (1 - 2 * m) * A + 2 * m * C if m < 0.5 else (2 - 2 * m) * C + (2 * m - 1) * B
for name, fn in [('linear', lin), ('bezier_via_hedz', bez), ('piecewise_via_hedz', pw)]:
    wav(f'dev/e2e/mask_proof/hol_{name}.wav', render(fn))
    print(name, 'written')
enc_mid = bez(0.5); r_worst = max(1 - math.exp(e[1]) for e in enc_mid)
print(f"bezier midpoint worst radius {r_worst:.6f} (stable by convexity, as the armadillo argument promises)")
