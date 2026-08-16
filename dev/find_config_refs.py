import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

TARGET_CONFIG_PTR = 0x20000380

print(f"Searching for references to config struct 0x{TARGET_CONFIG_PTR:08X}...")
pools = []
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    if w == TARGET_CONFIG_PTR:
        pool_addr = FLASH_BASE + i
        pools.append(pool_addr)
        print(f"Literal pool at 0x{pool_addr:08X}")

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

# Find code accessing these pools
for p_addr in pools:
    for c_pos in range(max(0, p_addr - FLASH_BASE - 1024), min(len(app_bin) - 4, p_addr - FLASH_BASE + 512), 2):
        c_addr = FLASH_BASE + c_pos
        chunk = app_bin[c_pos:c_pos+4]
        for insn in md.disasm(chunk, c_addr):
            if insn.mnemonic.startswith(('ldr', 'mov')) and len(insn.operands) > 1:
                op = insn.operands[1]
                if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                    t = ((insn.address + 4) & ~3) + op.mem.disp
                    if t == p_addr:
                        print(f"Code at 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")

