import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

def find_prologue(target_addr):
    # Scan backward for push {..., lr}
    for a in range(target_addr, target_addr - 2000, -2):
        chunk = app_bin[a - FLASH_BASE : a - FLASH_BASE + 4]
        for insn in md.disasm(chunk, a):
            if insn.mnemonic == 'push' or insn.mnemonic == 'push.w':
                if 'lr' in insn.op_str:
                    return a
    return target_addr

prologue = find_prologue(0x0803638E)
print(f"Prologue found at 0x{prologue:08X}")

chunk = app_bin[prologue - FLASH_BASE : 0x08036420 - FLASH_BASE]
for insn in md.disasm(chunk, prologue):
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

