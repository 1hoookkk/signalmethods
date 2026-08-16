import json

with open('plotdata/corpus_section_recurrence_report.json') as f:
    data = json.load(f)

print("SUMMARY:")
for k, v in data['summary'].items():
    print(f"  {k}: {v}")

print("\nTOP 12 RECURRING SECTIONS:")
for s in data['top_recurring_complete_sections'][:12]:
    print(f"Rank #{s['rank']}: {s['total_occurrences']} occ across {s['num_distinct_filters']} filters")
    print(f"  Words: {s['words']}")
    print(f"  Slots: {s['slots']}")
    print(f"  Corners: {s['corners']}")
    print(f"  Zero 39k: {s['zero_geometry_39k']}")
    print(f"  Pole 39k: {s['pole_geometry_39k']}")
    print(f"  Scale: {s['scale']:.4f}")
    print(f"  Filters: {s['filters']}")
    print("-" * 50)
