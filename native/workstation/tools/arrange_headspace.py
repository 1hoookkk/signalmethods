import json
from pathlib import Path
import numpy as np
from scipy.spatial.distance import cdist


def main():
    folder = Path(__file__).resolve().parents[1] / 'data'
    anchors = json.loads((folder / 'headspace-anchors.json').read_text())['anchors']
    chords = np.array([a['chord'] for a in anchors])
    blocks = [chords[:, :, 1], np.log(chords[:, :, 2]), chords[:, :, 6]]
    features = []
    scales = []
    for block in blocks:
        scale = block.std(axis=0)
        active = scale > 1e-10
        features.append((block[:, active] - block[:, active].mean(axis=0)) / scale[active] / np.sqrt(active.sum()))
        scales.append({'mean': block.mean(axis=0).tolist(), 'std': scale.tolist(), 'active': active.tolist()})
    features = np.concatenate(features, axis=1)
    cost = cdist(features, features, metric='sqeuclidean') / len(blocks)
    n = len(anchors)
    axial = hexagon(n)
    points = axial @ np.array([[1, 0], [.5, np.sqrt(3) / 2]])
    distance = cdist(points, points)
    edges = np.argwhere(np.triu(np.abs(distance - 1) < 1e-9, 1))
    triangles = [(a, b, c) for a in range(n) for b in range(a + 1, n) for c in range(b + 1, n)
                 if abs(distance[a, b] - 1) < 1e-9 and abs(distance[b, c] - 1) < 1e-9 and abs(distance[a, c] - 1) < 1e-9]
    incident = [np.where((edges == i).any(axis=1))[0] for i in range(n)]
    affected = {(i, j): np.unique(np.concatenate([incident[i], incident[j]])) for i in range(n) for j in range(i + 1, n)}
    rng = np.random.default_rng(19801995)
    objective = lambda order: float(cost[order[edges[:, 0]], order[edges[:, 1]]].sum())
    baselines = [objective(rng.permutation(n)) for _ in range(256)]
    best_cost = np.inf
    for restart in range(8):
        order = rng.permutation(n)
        total = objective(order)
        for step in range(100000):
            i, j = sorted(rng.choice(n, 2, replace=False))
            subset = edges[affected[i, j]]
            before = cost[order[subset[:, 0]], order[subset[:, 1]]].sum()
            order[i], order[j] = order[j], order[i]
            after = cost[order[subset[:, 0]], order[subset[:, 1]]].sum()
            delta = after - before
            temperature = 2.0 * (.002 / 2.0) ** (step / 99999)
            if delta <= 0 or rng.random() < np.exp(-delta / temperature):
                total += delta
            else:
                order[i], order[j] = order[j], order[i]
        improved = True
        while improved:
            improved = False
            for (i, j), indices in affected.items():
                subset = edges[indices]
                before = cost[order[subset[:, 0]], order[subset[:, 1]]].sum()
                order[i], order[j] = order[j], order[i]
                after = cost[order[subset[:, 0]], order[subset[:, 1]]].sum()
                if after < before - 1e-12:
                    improved = True
                else:
                    order[i], order[j] = order[j], order[i]
        total = objective(order)
        print(f'restart {restart + 1}: {total:.9f}', flush=True)
        if total < best_cost:
            best_cost, best = total, order.copy()
    anchor_points = np.empty_like(points)
    anchor_points[best] = points
    anchor_axial = np.empty_like(axial)
    anchor_axial[best] = axial
    result = {'schema': 'headspace-layout-v1', 'seed': 19801995, 'axial': anchor_axial.tolist(),
              'points': anchor_points.tolist(), 'triangles': (best[np.array(triangles)] + 1).tolist(),
              'names': [a['group'] + ': ' + a['name'] for a in anchors], 'featureBlocks': scales,
              'edgeCost': best_cost, 'randomMeanEdgeCost': float(np.mean(baselines)),
              'randomMinEdgeCost': min(baselines), 'edges': len(edges), 'restarts': 8, 'stepsPerRestart': 100000}
    (folder / 'headspace-layout.json').write_text(json.dumps(result, indent=2) + '\n')


def hexagon(n):
    best = None
    for q_hi in range(1, 13):
        for r_hi in range(1, 13):
            for s_lo in range(0, q_hi + r_hi + 1):
                for s_hi in range(s_lo, q_hi + r_hi + 1):
                    pts = [(q, r) for r in range(r_hi + 1) for q in range(q_hi + 1) if s_lo <= q + r <= s_hi]
                    if len(pts) != n:
                        continue
                    arr = np.array(pts)
                    xy = arr @ np.array([[1, 0], [.5, np.sqrt(3) / 2]])
                    d = cdist(xy, xy)
                    internal = int((np.abs(d - 1) < 1e-9).sum() // 2)
                    key = (internal, -q_hi, -r_hi, -s_lo, -s_hi)
                    if best is None or key > best[0]:
                        best = (key, arr)
    assert best is not None, 'no lattice hexagon with that many points'
    return best[1]


if __name__ == '__main__':
    main()
