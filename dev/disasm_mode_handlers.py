import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

def disasm_func(start_addr, count=50):
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

# Let's inspect the mode setting functions:
# 0x080321D6, 0x080322B6, 0x080323F2, 0x08032458, 0x0803257C, 0x080325C4, 0x0803271C, 0x08033E24, 0x08033E62, 0x08034242, 0x0803431E, 0x080343DE, 0x08034438, 0x08034558
addresses = [0x08032198, 0x080332B4, 0x08033D78, 0x08034230, 0x08034310, 0x080343D0, 0x08034428, 0x08034550]
for addr in addresses:
    print(f"\n{'='*60}\nAddress 0x{addr:08X}\n{'='*60}")
    print(disasm_func(addr, 25))

