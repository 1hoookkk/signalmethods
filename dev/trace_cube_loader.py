import capstone, struct, os, re

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# 1. Let's find the function that references "Cubes Successfully Loaded" or "Play Morpheus Cube .wav"
# Let's find the exact address where the string pointers are loaded
str_play_wav = 0x0803E4F8 # "Play Morpheus Cube .wav"
str_cubes_ok = 0x0803E544 # "Cubes Successfully Loaded"
str_not_cube = 0x0803E560 # "Not Morpheus Cube file"

print("Searching for functions referencing cube loader strings...")

# Look for literal pool words
loader_fns = set()
for i in range(0, len(app_bin) - 4, 4):
    w = struct.unpack('<I', app_bin[i:i+4])[0]
    if w in [str_play_wav, str_cubes_ok, str_not_cube]:
        pool_addr = FLASH_BASE + i
        print(f"Literal pool at 0x{pool_addr:08X} holds ptr to 0x{w:08X}")
        # Find code pointing to pool_addr
        for c_pos in range(max(0, i - 1024), min(len(app_bin) - 4, i + 512), 2):
            c_addr = FLASH_BASE + c_pos
            for insn in md.disasm(app_bin[c_pos:c_pos+4], c_addr, count=1):
                if insn.mnemonic == 'ldr' and len(insn.operands) > 1:
                    op = insn.operands[1]
                    if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                        t = ((insn.address + 4) & ~3) + op.mem.disp
                        if t == pool_addr:
                            print(f"  --> Referenced by 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")
                            loader_fns.add(insn.address & ~1)

# Disassemble the loader functions to find the unpacker / parser call
print("\nDisassembling cube loader functions:")
for fn_addr in sorted(loader_fns):
    start = max(FLASH_BASE, fn_addr - 128)
    end = min(FLASH_END, fn_addr + 512)
    chunk = app_bin[start - FLASH_BASE : end - FLASH_BASE]
    print(f"\n=== FUNCTION AROUND 0x{fn_addr:08X} ===")
    for insn in md.disasm(chunk, start):
        mark = "===>" if insn.address == fn_addr else "    "
        print(f"  {mark} 0x{insn.address:08X}: {insn.mnemonic:8s} {insn.op_str}")
