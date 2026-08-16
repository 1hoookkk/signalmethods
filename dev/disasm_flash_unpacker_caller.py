import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

def disasm_func(addr, length, label):
    print(f"\n================================================================================")
    print(f"DISASSEMBLY: {label} (0x{addr:08X} - 0x{addr+length:08X})")
    print(f"================================================================================")
    off = addr - base_addr
    for ins in md.disasm(firmware[off : off + length], addr):
        print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")

# 1. Disassemble around 0x08027EB0..0x08027F60
disasm_func(0x08027EB0, 160, "Caller 1: Flash Cube Record Loader (0x08027EB0)")

# 2. Disassemble around 0x08038650..0x08038720
disasm_func(0x08038650, 180, "Caller 2: Master Unpack Coordinator (0x08038650)")

# 3. Disassemble around 0x08039480..0x08039500
disasm_func(0x08039480, 160, "Caller 3: Audio Receive / Flash Writer Callback (0x08039480)")
