import math, struct, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
SR = 44100.0
def poles_of(body, ci):
    st = dl.decode_p2k_body(open(f'ref/presets/P2k_{body}.bin', 'rb').read(), SR)[ci]
    return [(g.pole.hz, g.pole.r) for g in st if g.pole.kind == 'conjugate' and g.pole.hz < 9000 and -math.log(g.pole.r) * SR / math.pi < 1500]
six = poles_of('013_talking_hedz', 0)
ten = sorted(set(six + poles_of('010_ooh_to_eee', 0) + poles_of('024_cruz_pusher', 1)))[:10]
def render(poles, path):
    cs = []
    for hz, r in poles:
        t = 2 * math.pi * hz / SR; a1, a2 = -2 * r * math.cos(t), r * r
        cs.append(((1 + a1 + a2), a1, a2))
    n = int(SR * 3); out = np.zeros(n); z = [[0.0, 0.0] for _ in poles]
    ph = 0.0
    for i in range(n):
        ph += 55.0 / SR
        if ph >= 1: ph -= 1
        x = 0.25 * (2 * ph - 1)
        for s, (dc, a1, a2) in enumerate(cs):
            w = x - a1 * z[s][0] - a2 * z[s][1]
            x = dc * w
            z[s][1] = z[s][0]; z[s][0] = w
        out[i] = x
    out /= max(np.max(np.abs(out)), 1e-9)
    d = (np.clip(out * 0.5, -1, 1) * 32767).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(d)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 1, int(SR), int(SR)*2, 2, 16) + b'data' + struct.pack('<I', len(d)) + d)
    print(path, len(poles), 'poles:', " ".join(f"{h:.0f}" for h, _ in poles))
render(six, 'dev/e2e/mask_proof/stages_6.wav')
render(ten, 'dev/e2e/mask_proof/stages_10.wav')
