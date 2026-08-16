import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

target = 0x08027CF8
print(f"Searching callers of 0x{target:08X}...")
for i in range(0, len(app_bin) - 4, 2):
    addr = FLASH_BASE + i
    chunk = app_bin[i:i+4]
    for insn in md.disasm(chunk, addr):
        if insn.mnemonic in ('bl', 'b.w', 'b') and len(insn.operands) > 0:
            if insn.operands[0].type == capstone.arm.ARM_OP_IMM and insn.operands[0].imm == target:
                print(f"Caller at 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")

