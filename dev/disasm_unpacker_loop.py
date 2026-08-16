import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

start_addr = 0x08038600
length = 80
offset = start_addr - base_addr

print(f"=== LOOP CONTROLLER OF UNPACKER (0x08038600 - 0x08038650) ===")
for ins in md.disasm(firmware[offset : offset + length], start_addr):
    print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")
