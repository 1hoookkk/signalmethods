import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

def disasm_range(start_addr, end_addr, title=""):
    print(f"\n{'='*80}\n{title} (0x{start_addr:08X}..0x{end_addr:08X})\n{'='*80}")
    chunk = app_bin[start_addr - FLASH_BASE : end_addr - FLASH_BASE]
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
        print(f"0x{insn.address:08X}:  {insn.bytes.hex():10s}  {insn.mnemonic:8s} {insn.op_str}{extra}")

# Check 0x08034BC0 and 0x08035122
disasm_range(0x08034B80, 0x08034C20, "Function around 0x08034BC0")
disasm_range(0x08035100, 0x08035180, "Function around 0x08035122")

# Also search for any 0x1A or 0x1B written to 0x60000000
print("\n" + "="*80)
print("Searching for commands 0x1A, 0x1B across binary...")
for i in range(0, len(app_bin) - 4, 2):
    addr = FLASH_BASE + i
    chunk = app_bin[i:i+4]
    for insn in md.disasm(chunk, addr):
        if insn.mnemonic.startswith('mov') and len(insn.operands) > 1:
            if insn.operands[1].type == capstone.arm.ARM_OP_IMM and insn.operands[1].imm in (0x1A, 0x1B):
                print(f"0x{insn.address:08X}:  {insn.mnemonic:8s} {insn.op_str}")

