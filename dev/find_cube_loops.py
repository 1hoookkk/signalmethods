import capstone, struct, os

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Let's search for instructions with immediate 332 (0x14c) or 289 (0x121) or loops over 7 stages
print("=== SEARCHING FOR CUBE PARSER / 332-BYTE RECORD LOOPS ===")
cube_parser_addrs = []

for pos in range(0, len(app_bin) - 4, 2):
    addr = FLASH_BASE + pos
    chunk = app_bin[pos : pos + 16]
    for insn in md.disasm(chunk, addr, count=1):
        op_str = insn.op_str.lower()
        if '#0x14c' in op_str or '#332' in op_str or '#0x121' in op_str or '#289' in op_str:
            print(f"Found cube constant at 0x{addr:08X}: {insn.mnemonic} {insn.op_str}")
            cube_parser_addrs.append(addr)

# Let's search for math loops referencing 7 (stages) or 8 (corners) or 56
print("\n=== SEARCHING FOR 7-STAGE / 8-CORNER LOOPS ===")
for pos in range(0, len(app_bin) - 4, 2):
    addr = FLASH_BASE + pos
    chunk = app_bin[pos : pos + 16]
    for insn in md.disasm(chunk, addr, count=1):
        op_str = insn.op_str.lower()
        # Look for loops checking stage counter < 7 or corner < 8
        if insn.mnemonic == 'cmp' and ('#7' in op_str or '#0x7' in op_str or '#56' in op_str or '#0x38' in op_str):
            # Check context around it
            # Print if near math operations (fadd, fmul, fcos, bl, etc.)
            ctx = app_bin[max(0, pos - 20) : min(len(app_bin), pos + 40)]
            ctx_dis = list(md.disasm(ctx, max(FLASH_BASE, addr - 20)))
            has_math = any(ci.mnemonic.startswith(('f', 'v', 'bl', 'mul', 'sdiv', 'udiv')) for ci in ctx_dis)
            if has_math:
                print(f"Candidate Stage Loop at 0x{addr:08X}: {insn.mnemonic} {insn.op_str}")
                for ci in ctx_dis:
                    mark = "-->" if ci.address == addr else "   "
                    print(f"   {mark} 0x{ci.address:08X}: {ci.mnemonic:8s} {ci.op_str}")
                print()
