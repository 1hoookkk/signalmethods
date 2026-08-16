import struct

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000

# Dump 0x08033BA0..0x08033C80 as 32-bit words
start = 0x08033BA0 - base_addr
words = struct.unpack('<40I', firmware[start : start + 160])

print("=== VCB1 FLASH DESCRIPTOR TABLE (0x08033BA0) ===")
for i, w in enumerate(words):
    addr = 0x08033BA0 + i * 4
    # Check if w points to firmware
    extra = ""
    if 0x08020000 <= w <= 0x08040000:
        target_off = w - 0x08020000
        target_bytes = firmware[target_off : target_off + 16]
        extra = f"-> string/data: {repr(target_bytes)}"
    print(f"0x{addr:08X}: 0x{w:08X} ({w:10d}) {extra}")
