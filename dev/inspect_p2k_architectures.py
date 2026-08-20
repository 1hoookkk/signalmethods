import json, glob, os

files = [
    'recipes/architectures/P2k_013_TalkingHedz.json',
    'recipes/architectures/P2k_020_Eeh-To-Aah.json',
    'recipes/architectures/P2k_000_Ace of Bass.json',
    'recipes/architectures/P2k_010_Ooh-To-Eee.json',
    'recipes/architectures/P2k_021_UbuOrator.json'
]

for p in files:
    if not os.path.exists(p):
        continue
    with open(p, 'r', encoding='utf-8') as f:
        data = json.load(f)
    print('='*80)
    print('PRESET:', data['name'])
    print('='*80)
    for si, s in enumerate(data['sections']):
        print(f'Stage S{si+1}:')
        for cname in ['M0_Q0', 'M100_Q0', 'M0_Q100', 'M100_Q100']:
            g = s['corners'][cname]
            p_data = g['pole']
            z_data = g['zero']
            p_str = f"RealPair {p_data['pair']}" if 'pair' in p_data else f"{p_data['hz']:7.1f}Hz r={p_data['r']:.4f}"
            z_str = f"RealPair {z_data['pair']}" if 'pair' in z_data else f"{z_data['hz']:7.1f}Hz r={z_data['r']:.4f}"
            print(f"  {cname:10s} | Pole: {p_str:25s} | Zero: {z_str:25s}")
