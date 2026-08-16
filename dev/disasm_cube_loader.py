import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

def disasm_range(start_addr, length, label=""):
    print(f"\n================================================================================")
    print(f"DISASSEMBLY: {label} (0x{start_addr:08X} - 0x{start_addr+length:08X})")
    print(f"================================================================================")
    offset = start_addr - base_addr
    code = firmware[offset : offset + length]
    for ins in md.disasm(code, start_addr):
        print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")

# 1. Disassemble around 0x08033BC0..0x08033D00 (VCB1 table & Flash Loader)
disasm_range(0x08033B80, 256, "VCB1 Header Descriptor & Flash Loader")

# 2. Disassemble around 0x08039DC0..0x08039F00 (VCB1 Parser / Audio Receive Handler)
disasm_range(0x08039DC0, 300, "VCB1 Audio Receiver / Parser")

# 3. Disassemble around 0x0803E4B0..0x0803E600 (UI Strings & Flash Loader Dispatch)
disasm_range(0x0803E400, 256, "Cube Loader Dispatch / UI Handler")
