import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

tbb_addr = 0x0802E96C
base = tbb_addr + 4
print(f"TBB table at 0x{base:08X}:")
for i in range(10):
    b = app_bin[base - FLASH_BASE + i]
    dest = base + (b * 2)
    print(f"Entry {i}: byte 0x{b:02X} -> target 0x{dest:08X}")

