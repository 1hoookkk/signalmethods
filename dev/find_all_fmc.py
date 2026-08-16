import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Search for all FMC base address literals: 0x60000000, 0x60040000, 0x60080000, etc.
print("Searching for FMC base address literals in literal pools...")
fmc_addrs = [0x60000000, 0x60040000, 0x60080000, 0x600C0000, 0x60000004, 0x60040004]
fmc_fns = set()

for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    if w in fmc_addrs:
        pool_addr = FLASH_BASE + i
        print(f"Literal at 0x{pool_addr:08X} = 0x{w:08X}")
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
                            fmc_fns.add(insn.address)

