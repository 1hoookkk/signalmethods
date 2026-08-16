import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

start_addr = 0x080382FC
length = 400
offset = start_addr - base_addr

print(f"=== FULL DISASSEMBLY OF CUBE UNPACKER 0x080382FC (FLASH RECORD -> RUNTIME DSP) ===")
for ins in md.disasm(firmware[offset : offset + length], start_addr):
    print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")
