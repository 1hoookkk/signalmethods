import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Disassemble from 0x08037780 to 0x08037AB0 to see the caller and stage 1 loader
chunk = app_bin[0x08037780 - FLASH_BASE : 0x08037B80 - FLASH_BASE]
print(f"Disassembly of 0x08037780..0x08037B80:")
for insn in md.disasm(chunk, 0x08037780):
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
