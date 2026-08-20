import json, math
from collections import defaultdict

with open('ref/morpheus/cubes_decoded.json', 'r', encoding='utf-8') as f:
    morph = json.load(f)

transposition_groups = defaultdict(list)

for c in morph['cubes']:
    cname = c['name']
    for ci, cor in enumerate(c['corners']):
        poles = []
        for s in cor['sections']:
            p = s['pole']
            if p['r'] > 0.45 and p['hz'] > 20.0:
                poles.append((p['hz'], round(p['r'], 3)))
        if len(poles) >= 3:
            poles.sort(key=lambda x: x[0])
            base_hz = poles[0][0]
            # Compute semitone intervals relative to base pole
            st_intervals = tuple(round(12.0 * math.log2(hz / base_hz), 1) for hz, r in poles[1:])
            radii = tuple(r for hz, r in poles)
            key = (st_intervals, radii)
            transposition_groups[key].append((cname, ci, round(base_hz, 1)))

reused_transpositions = {k: v for k, v in transposition_groups.items() if len(v) > 1}
print(f'Total corners with >=3 active poles: {sum(len(v) for v in transposition_groups.values())}')
print(f'Distinct pitch-invariant chord templates: {len(transposition_groups)}')
print(f'Transpositionally reused templates: {len(reused_transpositions)}')

top_trans = sorted(reused_transpositions.items(), key=lambda x: len(x[1]), reverse=True)
print('\n=== TOP 10 TRANSPOSITIONALLY REUSED ROOT CHORDS ===')
for (st_int, radii), clist in top_trans[:10]:
    distinct_cubes = set(x[0] for x in clist)
    print(f'Template: Intervals={st_int} | Radii={radii} | Corners={len(clist)} | Cubes={len(distinct_cubes)}')
    sample_instances = [f"{x[0]}.c{x[1]} @ {x[2]}Hz" for x in clist[:4]]
    print(f'  Instances: {sample_instances}')
    print()
