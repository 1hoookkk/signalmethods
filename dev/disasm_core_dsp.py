import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

def disasm_func(start_addr, count=100):
    chunk = app_bin[start_addr - FLASH_BASE : start_addr - FLASH_BASE + count * 4]
    lines = []
    for insn in md.disasm(chunk, start_addr):
        extra = ""
        if insn.mnemonic.startswith(('ldr', 'vldr')) and len(insn.operands) > 1:
            op = insn.operands[1]
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                t = ((insn.address + 4) & ~3) + op.mem.disp
                if FLASH_BASE <= t <= FLASH_END - 4:
                    val = struct.unpack('<I', app_bin[t - FLASH_BASE : t - FLASH_BASE + 4])[0]
                    fval = struct.unpack('<f', app_bin[t - FLASH_BASE : t - FLASH_BASE + 4])[0]
                    extra = f" // [0x{t:08X}] = 0x{val:08X} ({val}, f:{fval:g})"
        lines.append(f"0x{insn.address:08X}:  {insn.bytes.hex():10s}  {insn.mnemonic:8s} {insn.op_str}{extra}")
    return "\n".join(lines)

# 1. 0x0803919E / 0x080391D0 - FPGA Init Sequence
print("="*80)
print("FPGA INIT SEQUENCE (0x08039180 .. 0x08039300)")
print("="*80)
print(disasm_func(0x08039180, 100))

# 2. 0x08038B7E .. 0x08038D60 - DSP Streaming / FMC Commands
print("\n" + "="*80)
print("FMC STREAMING / COMMANDS (0x08038B60 .. 0x08038D60)")
print("="*80)
print(disasm_func(0x08038B60, 120))

# 3. 0x0802F280 .. 0x08030800 - Filter Calculation / Interpolation / Stage Loop
print("\n" + "="*80)
print("FILTER CALCULATION / FMC INTERPOLATION (0x0802F260 .. 0x0802F400)")
print("="*80)
print(disasm_func(0x0802F260, 100))
