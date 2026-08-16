import struct, os, capstone

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

# Let's search for the string "Cubes" or "Cube" or "AEParaVowel" or "Morpheus"
pos = 0
found_strings = []
while True:
    pos = app_bin.find(b'Cube', pos)
    if pos == -1: break
    addr = FLASH_BASE + pos
    # Extract string around it
    s_start = pos
    while s_start > 0 and 32 <= app_bin[s_start-1] <= 126:
        s_start -= 1
    s_end = pos
    while s_end < len(app_bin) and 32 <= app_bin[s_end] <= 126:
        s_end += 1
    s_str = app_bin[s_start:s_end].decode('ascii', errors='ignore')
    found_strings.append((FLASH_BASE + s_start, s_str))
    pos += 4

print("Found Cube-related strings:")
for addr, s in sorted(set(found_strings)):
    print(f"  0x{addr:08X}: \"{s}\"")

# Now let's search for where ANY of these string addresses are referenced in .rodata / .data tables!
print("\nSearching for pointer tables containing these addresses:")
menu_ptrs = {}
for s_addr, s in found_strings:
    b = struct.pack('<I', s_addr)
    p_pos = 0
    while True:
        p_pos = app_bin.find(b, p_pos)
        if p_pos == -1: break
        tbl_addr = FLASH_BASE + p_pos
        print(f"Table/Word at 0x{tbl_addr:08X} -> points to 0x{s_addr:08X} (\"{s}\")")
        menu_ptrs[tbl_addr] = (s_addr, s)
        p_pos += 4

# Now search for where menu_ptrs are referenced in code!
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

print("\nSearching for instructions loading menu tables:")
for tbl_addr, (s_addr, s) in menu_ptrs.items():
    b = struct.pack('<I', tbl_addr)
    p_pos = 0
    while True:
        p_pos = app_bin.find(b, p_pos)
        if p_pos == -1: break
        code_ref = FLASH_BASE + p_pos
        print(f"Code/Data at 0x{code_ref:08X} references table 0x{tbl_addr:08X}")
        p_pos += 4
