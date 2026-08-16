import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

regs = {
    'r0': 0, 'r1': 0, 'r2': 0, 'r3': 0x60040000, 'r4': 0x60000000,
    'r5': 0, 'r6': 0, 'r7': 0, 'r8': 0, 'sb': 0, 'sl': 0, 'fp': 0, 'ip': 0, 'lr': 0, 'sp': 0x20010000, 'pc': 0
}

writes = []

chunk = app_bin[0x0803919A - FLASH_BASE : 0x08039446 - FLASH_BASE]

for insn in md.disasm(chunk, 0x0803919A):
    m = insn.mnemonic
    ops = [op.strip() for op in insn.op_str.split(',')]
    
    # Handle mov / mov.w / movs
    if m.startswith('mov'):
        dst = ops[0]
        src = ops[1]
        if src.startswith('#'):
            regs[dst] = int(src[1:], 0)
        elif src in regs:
            regs[dst] = regs[src]
    # Handle ldr
    elif m.startswith('ldr') and len(ops) > 1 and ops[1].startswith('[pc'):
        dst = ops[0]
        # compute PC relative
        disp = insn.operands[1].mem.disp
        t = ((insn.address + 4) & ~3) + disp
        val = struct.unpack('<I', app_bin[t - FLASH_BASE : t - FLASH_BASE + 4])[0]
        regs[dst] = val
    # Handle strb / strb.w
    elif m.startswith('strb'):
        src = ops[0]
        dst_mem = ops[1] # e.g. [r4] or [r3]
        base_reg = dst_mem.strip('[]')
        base_addr = regs[base_reg]
        val = regs[src] & 0xFF
        writes.append((insn.address, base_addr, val, src))
    elif m.startswith('bl'):
        pass # skip function calls

print("=== EXACT SEQUENCE OF FMC WRITES IN FPGA INIT (0x08039180..0x08039446) ===")
last_cmd = None
for addr, base_addr, val, src in writes:
    if base_addr == 0x60000000:
        last_cmd = val
        print(f"\n[0x{addr:08X}] COMMAND REG 0x{val:02X} ({val:3d}) (from {src})")
    elif base_addr == 0x60040000:
        print(f"  [0x{addr:08X}]   DATA WRITE: 0x{val:02X} ({val:3d}) (cmd=0x{last_cmd:02X}, from {src})")
    else:
        print(f"  [0x{addr:08X}]   OTHER WRITE 0x{base_addr:08X}: 0x{val:02X}")

