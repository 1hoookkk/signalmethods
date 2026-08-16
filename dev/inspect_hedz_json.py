import json, math

with open('C:/Users/hooki/trench-x3-clean/dev/reference/p2k_vowel_isolated_sections_44100.json') as fp:
    vowels = json.load(fp)

hedz = next(p for p in vowels['presets'] if p['name'] == 'TalkingHedz')
print("TalkingHedz keys:", hedz.keys())
print("Corner keys in TalkingHedz:", hedz['corners'].keys())
c0 = hedz['corners']['M0_Q0']
print("Stages in M0_Q0:")
for s in c0['stages']:
    print(" ", s)
