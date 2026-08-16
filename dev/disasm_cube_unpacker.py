import struct

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
print(f"Loaded firmware image: {len(firmware)} bytes at base 0x{base_addr:08X}")

# 1. Search for strings: '_VCB1', 'VCB', 'Cube', 'Null', etc.
targets = [b'_VCB1', b'VCB', b'Cube', b'Null']
for t in targets:
    idx = 0
    while True:
        pos = firmware.find(t, idx)
        if pos == -1:
            break
        addr = base_addr + pos
        print(f"Found {t} at offset 0x{pos:05X} (Addr: 0x{addr:08X})")
        # Print surrounding 32 bytes
        print(f"   Context: {firmware[max(0, pos-16) : min(len(firmware), pos+32)]}")
        idx = pos + len(t)

# 2. Search for references to 332 (0x14C) or 320 (0x140) or 44 (0x2C) or 289 (0x121)
# In ARM Thumb-2:
# movw rX, #332 (0x014C) -> f240 1X4c or similar
# cmp rX, #289 (0x0121) -> f5bX 7X91 or cmp rX, #289
print("\n=== SEARCHING FOR CONSTANTS 332 (0x14C), 289 (0x121), 44 (0x2C) IN CODE ===")

# Search 16-bit and 32-bit integer literals in literal pools
for val, name in [(332, '332 (0x14C)'), (289, '289 (0x121)'), (44, '44 (0x2C)'), (320, '320 (0x140)'), (308, '308 (0x134)')]:
    packed16 = struct.pack('<H', val)
    packed32 = struct.pack('<I', val)
    
    idx = 0
    matches = []
    while True:
        pos = firmware.find(packed32, idx)
        if pos == -1:
            break
        matches.append(base_addr + pos)
        idx = pos + 4
        
    print(f"Literal 32-bit {name} matches at addresses: {[f'0x{a:08X}' for a in matches]}")
