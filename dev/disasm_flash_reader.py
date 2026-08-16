import struct
import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

# Search for 0x08040000 and 0x08060000 in literal pools
targets = [0x08040000, 0x08060000, 0x08040004, 0x08060004]
for t in targets:
    packed = struct.pack('<I', t)
    idx = 0
    while True:
        pos = firmware.find(packed, idx)
        if pos == -1:
            break
        addr = base_addr + pos
        print(f"Found reference to 0x{t:08X} in literal pool at 0x{addr:08X}")
        
        # Disassemble 128 bytes before and after to find the calling function
        start_dis = max(base_addr, addr - 128)
        offset = start_dis - base_addr
        code = firmware[offset : offset + 256]
        
        print(f"--- Disassembly around 0x{addr:08X} ---")
        for ins in md.disasm(code, start_dis):
            print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")
        print("\n" + "="*80 + "\n")
        
        idx = pos + 4
