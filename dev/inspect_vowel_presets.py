import json

with open('C:/Users/hooki/trench-x3-clean/dev/reference/p2k_vowel_isolated_sections_44100.json') as fp:
    vowels = json.load(fp)

print("Presets keys:")
for k in vowels['presets'].keys():
    print(" ", k)
