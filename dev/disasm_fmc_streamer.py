import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

def disasm_func(start_addr, count=60):
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

print("=== 0x080237C4 (FMC command sender / parameter streamer) ===")
print(disasm_func(0x080237C4, 40))

print("\n=== 0x08039588 (FMC command wrapper) ===")
print(disasm_func(0x08039588, 40))

# Dump tables at 0x0803E5CC, 0x0803E5E4, 0x0803E5FC, 0x0803E610, 0x0803E14C, 0x0803E624
tables = [0x0803E5CC, 0x0803E5E4, 0x0803E5FC, 0x0803E610, 0x0803E14C, 0x0803E624]
for t in tables:
    off = t - FLASH_BASE
    print(f"\n--- Table at 0x{t:08X} ---")
    data = app_bin[off : off + 24]
    print("Hex:", data.hex())
    words = [struct.unpack('<I', data[i:i+4])[0] for i in range(0, len(data), 4)]
    print("Words (uint32):", [f"0x{w:08X}" for w in words])

