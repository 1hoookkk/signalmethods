import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

start_addr = 0x08038A00
end_addr = 0x08039100

chunk = app_bin[start_addr - FLASH_BASE : end_addr - FLASH_BASE]

print(f"=== DISASSEMBLY OF CUBE TO DSP COEFFICIENT PIPELINE (0x{start_addr:08X}..0x{end_addr:08X}) ===")
for insn in md.disasm(chunk, start_addr):
    extra = ""
    if insn.mnemonic.startswith(('ldr', 'vldr')) and len(insn.operands) > 1:
        op = insn.operands[1]
        if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
            target_mem = ((insn.address + 4) & ~3) + op.mem.disp
            if FLASH_BASE <= target_mem < FLASH_BASE + len(app_bin) - 4:
                val = struct.unpack('<I', app_bin[target_mem - FLASH_BASE : target_mem - FLASH_BASE + 4])[0]
                extra = f" // [0x{target_mem:08X}] = 0x{val:08X}"
    print(f"0x{insn.address:08X}:  {insn.bytes.hex():10s}  {insn.mnemonic:8s} {insn.op_str}{extra}")
