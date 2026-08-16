import json

with open('C:/Users/hooki/trench-x3-clean/dev/reference/p2k_vowel_isolated_sections_44100.json') as fp:
    vowels = json.load(fp)

for p in vowels['presets']:
    print(f"Preset: {p.get('name', 'unnamed')} ({p.get('preset_key')})")
