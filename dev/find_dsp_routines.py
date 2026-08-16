import struct, re, os

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

# 1. Search for all 32-bit pointers in binary that point into .rodata (0x0803xxxx)
print("=== 1. POINTER TABLES IN BINARY ===")
pointers = {}
for i in range(0, len(app_bin) - 4, 4):
    val = struct.unpack('<I', app_bin[i:i+4])[0]
    if FLASH_BASE <= val < FLASH_END:
        target_offset = val - FLASH_BASE
        # Check if target is a null-terminated ASCII string
        s_bytes = bytearray()
        for b in app_bin[target_offset : min(len(app_bin), target_offset + 64)]:
            if b == 0: break
            if 32 <= b <= 126:
                s_bytes.append(b)
            else:
                break
        if len(s_bytes) >= 4:
            s_str = s_bytes.decode('ascii', errors='ignore')
            pointers[FLASH_BASE + i] = (val, s_str)
            if any(k in s_str for k in ['Cube', 'Dist', 'Pulse', 'Sync', 'Checksum', 'Morpheus', 'v1.0']):
                print(f"Ptr at 0x{FLASH_BASE + i:08X} -> 0x{val:08X}: \"{s_str}\"")

# 2. Read full disassembly and find functions referencing these pointer tables or addresses
print("\n=== 2. SEARCHING DISASSEMBLY FOR STRING / TABLE REFERENCES ===")
asm_path = 'dev/disasm/vulcan_full.asm'
with open(asm_path, 'r', encoding='utf-8') as fp:
    asm_lines = fp.readlines()

for line in asm_lines:
    # Check if line contains any interesting address or opcode
    for p_addr, (s_addr, s_text) in list(pointers.items())[:50]:
        if f"{p_addr:x}" in line.lower() or f"{s_addr:x}" in line.lower():
            print(f"{line.strip()}   // Ref: \"{s_text}\"")

# 3. Search for Math Constants & Sample Rates in .rodata
print("\n=== 3. SEARCHING FOR DSP CONSTANTS / SAMPLE RATES ===")
# 39062.5 Hz in float32 / int / hex
# float32 39062.5 = 0x47189A00
# float32 48000.0 = 0x473B8000
# float32 44100.0 = 0x472C4400
# float32 2*pi = 6.283185307 = 0x40C90FDB
# float32 pi   = 3.141592653 = 0x40490FDB
# float64 39062.5 = 0x40E3134000000000
# int 39063 = 0x00009897
# int 39062 = 0x00009896
# int 48000 = 0x0000BB80

constants = {
    struct.pack('<f', 39062.5): "39062.5f (Float32 Legacy Datum)",
    struct.pack('<f', 48000.0): "48000.0f (Float32 Vulcan Rate)",
    struct.pack('<f', 44100.0): "44100.0f (Float32 Rate)",
    struct.pack('<f', 3.1415926535): "pi (Float32)",
    struct.pack('<f', 6.283185307): "2*pi (Float32)",
    struct.pack('<I', 39062): "39062 (Int)",
    struct.pack('<I', 39063): "39063 (Int)",
    struct.pack('<I', 48000): "48000 (Int)",
    struct.pack('<I', 44100): "44100 (Int)",
}

for c_bytes, c_desc in constants.items():
    pos = 0
    while True:
        pos = app_bin.find(c_bytes, pos)
        if pos == -1: break
        c_addr = FLASH_BASE + pos
        print(f"Found {c_desc} at Flash Addr 0x{c_addr:08X} (offset 0x{pos:X})")
        pos += len(c_bytes)
