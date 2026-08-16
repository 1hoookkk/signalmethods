import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Let's search for 320 (0x140) or 332 (0x14C) or 288 (0x120) or 36 (0x24) in the entire binary
print("Searching for constants related to cube size (320, 332, 288, 36)...")
for i in range(0, len(app_bin) - 4, 2):
    addr = FLASH_BASE + i
    chunk = app_bin[i:i+4]
    for insn in md.disasm(chunk, addr):
        # Look for movw, mov, ldr, cmp, add, mul with 320, 332, etc.
        for op in insn.operands:
            if op.type == capstone.arm.ARM_OP_IMM and op.imm in (320, 332, 288, 36, 0x140, 0x14C, 0x120):
                print(f"0x{insn.address:08X}:  {insn.mnemonic:8s} {insn.op_str}")

