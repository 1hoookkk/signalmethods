import json

with open('C:/Users/hooki/trench-x3-clean/dev/reference/p2k_vowel_isolated_sections_44100.json') as fp:
    vowels = json.load(fp)

hedz = next(p for p in vowels['presets'] if p['name'] == 'TalkingHedz')
print(f"TalkingHedz has {len(hedz['sections'])} sections:")
for s in hedz['sections']:
    print(f"  Stage {s['stage']}: {s['corners']['M0_Q0']}")
