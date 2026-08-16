import json

with open('plotdata/corpus_section_recurrence_report.json') as f:
    data = json.load(f)

print('=== SUMMARY ===')
print(json.dumps(data['summary'], indent=2))

print('\n=== TOP RECURRING COMPLETE SECTIONS (Stored Words) ===')
for s in data['top_recurring_complete_sections'][:15]:
    print(f"Rank #{s['rank']}: {s['total_occurrences']} occurrences across {s['num_distinct_filters']} filters")
    print(f"  Words: {s['words']}")
    print(f"  Slots: {s['slots']}")
    print(f"  Corners: {s['corners']}")
    z = s['zero_geometry_39k']
    p = s['pole_geometry_39k']
    print(f"  Decoded 39k: Zero={z} | Pole={p} | Scale={s['scale']:.4f} ({20*np.log10(s['scale']) if 'np' in globals() else ''})")
    print(f"  Filters: {s['filters'][:6]}")
    print()

print('\n=== TOP RECURRING ZERO PAIRS ===')
for z in data['top_recurring_zeros'][:15]:
    print(f"Rank #{z['rank']}: {z['occurrences']} occ across {z['filters']} filters | Slots: {z['slots']}")
    print(f"  Words: {z['words']} | Geo: {z['geometry']}")
    print()

print('\n=== SHARED FULL CORNERS (Endpoints) ===')
for c in data['shared_full_corners'][:10]:
    print(f"Rank #{c['rank']}: {c['occurrences']} endpoints | Active stages: {c['active_stages']}")
    print(f"  Endpoints: {c['endpoints']}")
    print()

print('\n=== 5 SHARED + 1 REPLACED SECTION PAIRS ===')
for p in data['shared_5_replaced_1_pairs'][:10]:
    print(f"Pair #{p['pair_idx']}: {p['corner1']} <-> {p['corner2']} (Diff at S{p['diff_slot']})")
    print(f"  S{p['diff_slot']} A: {p['s1_words']} => {p['s1_geo']}")
    print(f"  S{p['diff_slot']} B: {p['s2_words']} => {p['s2_geo']}")
    print()

print('\n=== RECURRING EMULATOR X DESIGNER SECTIONS ===')
for x in data['emulator_x_recurring_designer_sections'][:15]:
    print(f"XML Rank #{x['rank']}: {x['occurrences']} occ across {x['filters_count']} presets | Slots: {x['slots']}")
    print(f"  Tuple: {x['designer_tuple']}")
    print(f"  Presets: {x['filters'][:5]}")
    print()
