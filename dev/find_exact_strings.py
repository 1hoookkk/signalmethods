import struct, os

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

# Let's extract all null-terminated strings and their exact addresses
pos = 0x0803D000 - FLASH_BASE
end_pos = len(app_bin)

print("Exact null-terminated strings in .rodata:")
strings_map = {}
while pos < end_pos:
    if app_bin[pos] != 0:
        s_start = pos
        while pos < end_pos and app_bin[pos] != 0:
            pos += 1
        s_bytes = app_bin[s_start:pos]
        if all(32 <= b <= 126 or b in (10, 13, 9) for b in s_bytes):
            s_addr = FLASH_BASE + s_start
            s_str = s_bytes.decode('ascii')
            strings_map[s_addr] = s_str
            if len(s_str) >= 4 and any(k in s_str for k in ['Cube', 'Load', 'WAV', 'Morpheus', 'Dist']):
                print(f"  0x{s_addr:08X}: \"{s_str}\"")
    pos += 1

# Now search for references to ANY of these exact string addresses!
print("\nSearching for references to exact string addresses:")
for s_addr, s_str in strings_map.items():
    b = struct.pack('<I', s_addr)
    p = 0
    while True:
        p = app_bin.find(b, p)
        if p == -1: break
        ref_addr = FLASH_BASE + p
        print(f"Found pointer to 0x{s_addr:08X} (\"{s_str}\") at 0x{ref_addr:08X}")
        p += 4
