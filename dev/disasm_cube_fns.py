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

# Let's inspect 0x08022AE0, 0x08027EB0, 0x08033F50
disasm_range(0x08022AC0, 0x08022B40, "Cube table / load 0x08022AC0")
disasm_range(0x08027E90, 0x08027F20, "Cube unpacking / loading 0x08027E90")
disasm_range(0x08033F50, 0x08034000, "0x08033F50 Mode 6 / unpack")
