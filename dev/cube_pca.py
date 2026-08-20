import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'cell_dictionary'))
import decode_lib as dl

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 39062.5
N_FREQ = 256
FREQS = np.array([40 * (16000 / 40) ** (i / (N_FREQ - 1)) for i in range(N_FREQ)])
GRID_POINTS = 11

def response_at(corners, m, q, z):
    n = len(corners)
    db = np.zeros(N_FREQ)
    for fi, f in enumerate(FREQS):
        total = 0
        for ci in range(n):
            wm = m if (ci & 1) else (1 - m)
            wq = q if (ci & 2) else (1 - q)
            wz = (z if (ci & 4) else (1 - z)) if n == 8 else 1.0
            w = wm * wq * wz
            if w < 1e-6:
                continue
            stage_sum = 0
            for geom in corners[ci]:
                if dl.stage_is_identity(geom):
                    continue
                gg = geom if geom.scale is not None else dl.StageGeometry(geom.pole, geom.zero, 1.0)
                bq = dl.stage_biquad(gg, SR)
                stage_sum += dl.stage_response_db(bq, [f], SR)[0]
            total += w * stage_sum
        db[fi] = total
    return db

def analyze_cube(cube_data, cid, name):
    corners_raw = cube_data['corners']
    n_corners = len(corners_raw)
    is_square = name.endswith('.4') or n_corners <= 4

    corners = []
    for corner in corners_raw:
        sections = corner['sections']
        geoms = []
        for sec in sections:
            p = sec['pole']
            z = sec['zero']
            pole = dl.Conjugate(p['hz'], p['r']) if p['r'] > 0.001 else dl.Degenerate()
            zero = dl.Conjugate(z['hz'], z['r']) if z['r'] > 0.001 else dl.Degenerate()
            scale = corner.get('gain', 1.0) if len(geoms) == 0 else 1.0
            geoms.append(dl.StageGeometry(pole, zero, scale))
        corners.append(geoms)

    positions = []
    spectra = []
    g = GRID_POINTS
    z_range = [0.0] if is_square else np.linspace(0, 1, g)
    for mi in np.linspace(0, 1, g):
        for qi in np.linspace(0, 1, g):
            for zi in z_range:
                db = response_at(corners, mi, qi, zi)
                positions.append((mi, qi, zi))
                spectra.append(db)

    X = np.array(spectra)
    mean = X.mean(axis=0)
    Xc = X - mean
    U, S, Vt = np.linalg.svd(Xc, full_matrices=False)
    total_var = np.sum(S ** 2)
    explained = (S ** 2) / total_var * 100

    return {
        'id': cid,
        'name': name,
        'n_corners': n_corners,
        'n_samples': len(spectra),
        'pc1': round(float(explained[0]), 1),
        'pc2': round(float(explained[1]), 1) if len(explained) > 1 else 0,
        'pc3': round(float(explained[2]), 1) if len(explained) > 2 else 0,
        'cumulative_3': round(float(sum(explained[:3])), 1),
        'effective_dims': int(np.sum(np.cumsum(explained) < 99)) + 1,
    }

with open(os.path.join(REPO, 'ref', 'morpheus', 'cubes_decoded.json')) as f:
    data = json.load(f)
cubes = data['cubes']

print(f'PCA analysis of {len(cubes)} Morpheus cubes')
print(f'Grid: {GRID_POINTS} points per axis, {N_FREQ} frequency bins')
print(f'{"ID":>4}  {"Name":<16}  {"PC1":>5}  {"PC2":>5}  {"PC3":>5}  {"Sum3":>5}  {"Dims":>4}')
print('-' * 65)

results = []
for cid in range(len(cubes)):
    c = cubes[cid]
    name = c['name']
    all_idle = all(
        all(s['pole']['r'] < 0.01 and s['zero']['r'] < 0.01 for s in corner['sections'])
        for corner in c['corners']
    )
    if all_idle:
        r = {'id': cid, 'name': name, 'pc1': 0, 'pc2': 0, 'pc3': 0, 'cumulative_3': 0, 'effective_dims': 0}
    else:
        try:
            r = analyze_cube(c, cid, name)
        except Exception as e:
            r = {'id': cid, 'name': name, 'pc1': -1, 'pc2': -1, 'pc3': -1, 'cumulative_3': -1, 'effective_dims': -1, 'error': str(e)}
    results.append(r)
    print(f'{cid:>4}  {name:<16}  {r["pc1"]:>5.1f}  {r["pc2"]:>5.1f}  {r["pc3"]:>5.1f}  {r["cumulative_3"]:>5.1f}  {r.get("effective_dims",0):>4}')
    if cid % 50 == 49:
        sys.stdout.flush()

print(f'\n{"=" * 65}')
dims = [r['effective_dims'] for r in results if r['effective_dims'] >= 0]
print(f'Effective dimensions: mean={np.mean(dims):.1f}, median={np.median(dims):.0f}')
print(f'  1D: {sum(1 for d in dims if d <= 1)}')
print(f'  2D: {sum(1 for d in dims if d == 2)}')
print(f'  3D: {sum(1 for d in dims if d == 3)}')
print(f'  4D+: {sum(1 for d in dims if d > 3)}')
pc1s = [r['pc1'] for r in results if r['pc1'] > 0]
print(f'PC1 explains: mean={np.mean(pc1s):.1f}%, median={np.median(pc1s):.1f}%')

with open(os.path.join(REPO, 'dev', 'cube_pca_results.json'), 'w') as f:
    json.dump(results, f, indent=1)
print(f'\nSaved to dev/cube_pca_results.json')
