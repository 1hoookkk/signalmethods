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

# Search for strings around 0x0803DBC0..0x0803DC50
print("=== STRINGS AROUND 0x0803DBC0 ===")
for a in range(0x0803DBA0, 0x0803DC50, 4):
    off = a - FLASH_BASE
    # Check if ascii string
    s = app_bin[off : off + 32].split(b'\x00')[0]
    try:
        dec = s.decode('ascii')
        if len(dec) > 2:
            print(f"0x{a:08X}: \"{dec}\"")
    except:
        pass

# Find literal pool references to 0x0803DBA0..0x0803DC50
print("\n=== LITERAL POOL REFERENCES ===")
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    if 0x0803DBA0 <= w <= 0x0803DC50:
        pool_addr = FLASH_BASE + i
        print(f"Literal pool at 0x{pool_addr:08X} -> 0x{w:08X}")
        # Look for code referencing pool_addr
        for c_pos in range(max(0, i - 1024), min(len(app_bin) - 4, i + 512), 2):
            c_addr = FLASH_BASE + c_pos
            chunk = app_bin[c_pos:c_pos+4]
            for insn in md.disasm(chunk, c_addr):
                if insn.mnemonic.startswith(('ldr', 'mov')) and len(insn.operands) > 1:
                    op = insn.operands[1]
                    if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                        t = ((insn.address + 4) & ~3) + op.mem.disp
                        if t == pool_addr:
                            print(f"  --> Ref by 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")

