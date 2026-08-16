import struct

with open('dev/disasm/vulcan_app_08020000.bin', 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

# Look for OLED commands (SSD1306/SH1106 initialization bytes: 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14...)
ssd1306_init = bytes([0xAE, 0xD5, 0x80, 0xA8])
pos = app_bin.find(b'\xae\xd5')
if pos != -1:
    print(f"SSD1306 init found at offset 0x{pos:08X} (0x{FLASH_BASE + pos:08X})")

# Look for cube projection constants / 128x64 framebuffer
print(f"Searching for 1024-byte (128x64/8) buffer clear or blit...")
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack_from('<I', app_bin, i)[0]
    if w == 1024 or w == 512: # 128x64 or 128x32 display buffer
        # check if near loop or memset
        pass
