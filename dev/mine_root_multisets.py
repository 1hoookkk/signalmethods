import json
from collections import defaultdict

with open('ref/morpheus/cubes_decoded.json', 'r', encoding='utf-8') as f:
    morph = json.load(f)

corners = []
for c in morph['cubes']:
    cname = c['name']
    for ci, cor in enumerate(c['corners']):
        poles = []
        zeros = []
        for s in cor['sections']:
            p = s['pole']
            z = s['zero']
            if p['r'] > 0.05:
                poles.append((round(p['hz'], 1), round(p['r'], 4)))
            if z['r'] > 0.05:
                zeros.append((round(z['hz'], 1), round(z['r'], 4)))
        poles.sort()
        zeros.sort()
        corners.append({
            'cube': cname,
            'corner_idx': ci,
            'poles': tuple(poles),
            'zeros': tuple(zeros),
            'n_poles': len(poles),
            'n_zeros': len(zeros)
        })

print(f'Total Morpheus corners: {len(corners)}')

# Group by exact pole set
pole_groups = defaultdict(list)
full_groups = defaultdict(list)

for c in corners:
    if c['n_poles'] > 0:
        ref_str = f"{c['cube']}.c{c['corner_idx']}"
        pole_groups[c['poles']].append(ref_str)
        full_groups[(c['poles'], c['zeros'])].append(ref_str)

print(f'Distinct pole multisets: {len(pole_groups)}')
print(f'Distinct full (pole+zero) multisets: {len(full_groups)}')

# Non-trivial multisets (more than 1 active root)
reused_full = {k: v for k, v in full_groups.items() if len(v) > 1 and (len(k[0]) + len(k[1]) > 2)}
print(f'Reused complex root sets (>1 corner, >2 roots): {len(reused_full)}')

top_full = sorted(reused_full.items(), key=lambda x: len(x[1]), reverse=True)
print('\n=== TOP 10 REUSED EXACT ROOT CONFIGURATIONS (Order-Independent) ===')
for (pset, zset), clist in top_full[:10]:
    distinct_cubes = set(x.split('.')[0] for x in clist)
    print(f'RootSet: {len(pset)} poles, {len(zset)} zeros | Corners: {len(clist):3d} | Distinct Presets: {len(distinct_cubes):2d}')
    print(f'  Presets: {list(distinct_cubes)[:8]}')
    print(f'  Poles: {pset[:3]} ...')
    print(f'  Zeros: {zset[:3]} ...')
    print()
