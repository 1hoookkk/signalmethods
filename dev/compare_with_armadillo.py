import struct, math

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

def decode_minifloat_u16(word):
    u = (word + 1) & 0xFFFF
    if u == 0 or u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    if e == 0:
        x = m / 4096.0
    else:
        x = (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))

# Preset 0 C0 bytes (36 bytes)
c0_bytes = app_bin[0x08008000 - FLASH_BASE + 44 : 0x08008000 - FLASH_BASE + 44 + 36]

# Let's inspect the 18 u16 words in 36 bytes:
u16_words = struct.unpack('<18H', c0_bytes)
print("="*80)
print("PRESET 0 CORNER 0 — 18 RAW u16 WORDS IN 36 BYTES:")
print("="*80)
for i, w in enumerate(u16_words):
    fval = decode_minifloat_u16(w)
    print(f"w{i:02d}: 0x{w:04X} ({w:5d}) -> minifloat: {fval:11.8f}")
