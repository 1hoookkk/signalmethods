import json, glob, os, math
from collections import defaultdict, Counter

sys_path = 'ref/cubes_289_names.json'
cube_names = json.load(open(sys_path))

# Load P2K
p2k_files = glob.glob('recipes/architectures/*.json')
p2k_data = [json.load(open(f)) for f in p2k_files]

def classify_name(name):
    n = name.lower()
    
    # 1. Vocal & Formant Morphs
    if any(k in n for k in ['vow', 'voce', 'hedz', 'talk', 'be-ye', 'ee-yi', 'ii-yi', 'uhr', 'boote', 'orator', 'bouche', 'ooh', 'eeh', 'aah', 'yeah', 'yah', 'yoyo', 'eoae', 'aeiou', 'vcl', 'sing', 'choir', 'throat', 'speech', 'formant', 'deepbouche', 'talkinghedz', 'multiqvox', 'fuzziface']):
        return '1. Vocal & Formant Morphs'
    
    # 2. Swept Multi-Pole Lowpass
    if any(k in n for k in ['lp', 'low', 'past', 'sweep', '4pole', '2pole', '6pole', 'sub', 'bass', 'acid', 'klub', 'mega', 'moog', 'roller', 'ladder', 'depth', 'megasweepz', 'klubklassik', 'acidravage', 'bassbox-303', 'bassomatic', 'bolandbass']):
        return '2. Swept Multi-Pole Lowpass'
        
    # 3. Multi-Notch Comb & Phasers
    if any(k in n for k in ['pha', 'comb', 'flng', 'flange', 'tooth', 'bender', 'notc', 'bat', 'spin', 'rotat', 'whirl', 'earbender', 'toothcomb', 'angelzhairz', 'dreamweava']):
        return '3. Multi-Notch Comb & Phasers'
        
    # 4. Contrary & Multi-Bandpass
    if any(k in n for k in ['bp', 'band', 'contrary', 'dualbp', 'triple', 'bpass', 'contrarybp', 'dualbpass', 'contrarybandpass']):
        return '4. Contrary & Multi-Bandpass'
        
    # 5. Highpass & Boundary Shelves
    if any(k in n for k in ['hp', 'high', 'shelf', 'hipass', 'bright', 'sizzle', 'air', 'fizz', 'earlyrizer', 'millennium', 'deadringer']):
        return '5. Highpass & Boundary Shelves'
        
    # 6. Swept Parametric EQ & Resonant Bells
    if any(k in n for k in ['eq', 'prmtrc', 'bell', 'boost', 'peak', 'tilt', 'tone', 'harm', 'zoompeaks', 'roguehertz', 'razorblades', 'radiocraze', 'freakshifta', 'cruzpusher', 'lucifersq']):
        return '6. Swept Parametric EQ & Resonant Bells'
        
    # 7. Modal, Metallic & Inharmonic Resonators
    if any(k in n for k in ['chime', 'metal', 'glass', 'piano', 'wire', 'ring', 'matrix', 'tine', 'reed', 'tube', 'klangkling', 'meatygizmo', 'tb-ornot-tb']):
        return '7. Modal, Metallic & Inharmonic Resonators'
        
    return '8. Hybrid & Exotic Transforms'

cube_classes = defaultdict(list)
for c in cube_names:
    cat = classify_name(c['name'])
    cube_classes[cat].append(c)

p2k_classes = defaultdict(list)
for p in p2k_data:
    cat = classify_name(p['name'])
    p2k_classes[cat].append(p)

print("=" * 80)
print("THE 7 CANONICAL ARCHETYPES OF THE Z-PLANE FILTER UNIVERSE")
print(f"Audited: {len(cube_names)} Morpheus 3D Cubes + {len(p2k_data)} P2K Factory Architectures")
print("=" * 80)

for cat in sorted(cube_classes.keys()):
    c_list = cube_classes[cat]
    p_list = p2k_classes.get(cat, [])
    total_filters = len(c_list) + len(p_list)
    print(f"\n### {cat}")
    print(f"Total Filters: {total_filters} ({len(c_list)} Cubes, {len(p_list)} P2K)")
    sample_c = [f"#{c['id']} {c['name']}" for c in c_list[:6]]
    print(f"Sample Cubes : {sample_c}")
    if p_list:
        print(f"P2K Presets  : {[p['name'] for p in p_list]}")
