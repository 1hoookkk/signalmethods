import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Let's trace symbolic execution / register state in 0x0803919A .. 0x08039446
regs = {f'r{i}': 0 for i in range(13)}
regs['r4'] = 0x60000000
regs['r3'] = 0x60040000
regs['sp'] = 0x20010000
regs['pc'] = 0
regs['lr'] = 0

chunk = app_bin[0x0803919A - FLASH_BASE : 0x08039448 - FLASH_BASE]
print("=== FPGA INIT TRACE (Register Writes to FMC) ===")
# Disassemble and log writes
for insn in md.disasm(chunk, 0x0803919A):
    # Print the instruction
    # If store to r4 (0x60000000) or r3 (0x60040000)
    print(f"0x{insn.address:08X}:  {insn.mnemonic:8s} {insn.op_str}")

