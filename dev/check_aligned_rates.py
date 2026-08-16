import struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

print("=== SEARCHING FOR ALIGNED 32-BIT RATE WORDS IN .TEXT / .RODATA ===")
for pos in range(0, len(app_bin) - 4, 4):
    val_u32 = struct.unpack('<I', app_bin[pos:pos+4])[0]
    val_f32 = struct.unpack('<f', app_bin[pos:pos+4])[0]
    
    if val_u32 in (48000, 44100, 39062, 39063, 96000, 88200):
        print(f"Aligned Uint32 {val_u32} at 0x{FLASH_BASE + pos:08X} (offset 0x{pos:X})")
    if abs(val_f32 - 48000.0) < 1.0 or abs(val_f32 - 39062.5) < 1.0 or abs(val_f32 - 44100.0) < 1.0:
        print(f"Aligned Float32 {val_f32} at 0x{FLASH_BASE + pos:08X} (offset 0x{pos:X})")
