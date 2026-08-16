import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

addr_48k = 0x0803757D
print(f"=== DISASSEMBLY AROUND LITERAL 48000 (0x{addr_48k:08X}) ===")
# Find function start
fn_start = 0x08037500
for a in range(addr_48k, 0x08037000, -2):
    t_off = a - FLASH_BASE
    chunk = app_bin[t_off : t_off + 4]
    for insn in md.disasm(chunk, a, count=1):
        if insn.mnemonic.startswith('push') and 'lr' in insn.op_str:
            fn_start = a
            break
    if fn_start != 0x08037500: break

print(f"Function starts at 0x{fn_start:08X}:")
for a in range(fn_start, min(fn_start + 256, FLASH_BASE + len(app_bin)), 2):
    t_off = a - FLASH_BASE
    chunk = app_bin[t_off : t_off + 4]
    for insn in md.disasm(chunk, a, count=1):
        extra = ""
        if insn.mnemonic.startswith(('ldr', 'vldr')) and len(insn.operands) > 1:
            op = insn.operands[1]
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                t = ((insn.address + 4) & ~3) + op.mem.disp
                if FLASH_BASE <= t < FLASH_BASE + len(app_bin) - 4:
                    val = struct.unpack('<I', app_bin[t - FLASH_BASE : t - FLASH_BASE + 4])[0]
                    extra = f" // [0x{t:08X}] = 0x{val:08X}"
        print(f"0x{insn.address:08X}:  {insn.bytes.hex():10s}  {insn.mnemonic:8s} {insn.op_str}{extra}")
