import struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

addrs = [0x0803E710, 0x0803E728, 0x0803E740, 0x0803E238, 0x0803E11C, 0x0803E3F8]
for a in addrs:
    off = a - FLASH_BASE
    # read null terminated ascii string
    end = app_bin.find(b'\x00', off)
    s = app_bin[off:end].decode('latin1', errors='replace')
    print(f"0x{a:08X}: \"{s}\"")

