bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

# Search for "Talking" or inspect preset table
pos = 0
found = []
while True:
    idx = app_bin.find(b'Talking', pos)
    if idx == -1:
        break
    found.append((idx, app_bin[idx:idx+32]))
    pos = idx + 1

print(f"Found 'Talking' matches in binary:")
for offset, snippet in found:
    addr = FLASH_BASE + offset
    print(f"  Offset 0x{offset:08X} (Addr 0x{addr:08X}): {snippet}")

# Check other preset names around 0x08008000 or in the binary
print("\nScanning for preset table names in binary:")
for i in range(0, len(app_bin) - 332, 16):
    s = app_bin[i:i+12]
    if all(32 <= b <= 126 for b in s) and s.strip() in [b'TalkingHedz', b'Vowel Morph', b'AEParLPVow', b'E-Mu', b'Morpheus']:
        print(f"  Preset match at offset 0x{i:08X} (0x{FLASH_BASE + i:08X}): {s}")
