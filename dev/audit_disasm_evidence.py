import capstone, struct, os

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Disassemble all instructions
instructions = {}
pc = 0
while pc < len(app_bin) - 2:
    addr = FLASH_BASE + pc
    try:
        chunk = app_bin[pc : min(pc + 16, len(app_bin))]
        dis = list(md.disasm(chunk, addr, count=1))
        if dis:
            insn = dis[0]
            instructions[addr] = insn
            pc += insn.size
        else:
            pc += 2
    except:
        pc += 2

print(f"Total instructions: {len(instructions)}")

# 1. TRACE 0x08038F68 LOOP
print("\n" + "="*80)
print("ITEM 1: LOOP AT 0x08038F68 & ENCLOSING FUNCTION")
print("="*80)
for a in range(0x08038F00, 0x08039030, 2):
    if a in instructions:
        i = instructions[a]
        extra = ""
        if i.mnemonic.startswith(('ldr', 'vldr')) and len(i.operands) > 1:
            op = i.operands[1]
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                t = ((i.address + 4) & ~3) + op.mem.disp
                if FLASH_BASE <= t < FLASH_END - 4:
                    val = struct.unpack('<I', app_bin[t - FLASH_BASE : t - FLASH_BASE + 4])[0]
                    extra = f" // [0x{t:08X}] = 0x{val:08X}"
        print(f"0x{i.address:08X}:  {i.bytes.hex():10s}  {i.mnemonic:8s} {i.op_str}{extra}")

# 2. TRACE 0x0803954C FUNCTION & CALLERS
print("\n" + "="*80)
print("ITEM 2: FUNCTION AT 0x0803953C / 0x08039588 AND CALLERS")
print("="*80)
for a in range(0x0803953C, 0x08039610, 2):
    if a in instructions:
        i = instructions[a]
        print(f"0x{i.address:08X}:  {i.bytes.hex():10s}  {i.mnemonic:8s} {i.op_str}")

# Find all callers of 0x0803953C, 0x08039544, 0x08039588
print("\nCallers of 0x0803953C / 0x08039544 / 0x08039588:")
for addr, insn in instructions.items():
    if insn.mnemonic in ('bl', 'blx', 'b', 'b.w'):
        for op in insn.operands:
            if op.type == capstone.arm.ARM_OP_IMM and op.imm in (0x0803953C, 0x08039544, 0x08039588):
                print(f"  Caller at 0x{addr:08X}: {insn.mnemonic} {insn.op_str}")
                # Print 16 bytes before call
                for ctx_a in range(addr - 20, addr + 4, 2):
                    if ctx_a in instructions:
                        ci = instructions[ctx_a]
                        m = "-->" if ci.address == addr else "   "
                        print(f"    {m} 0x{ci.address:08X}: {ci.mnemonic:8s} {ci.op_str}")

# 3. ALL WRITES TO 0x60000000 AND 0x60040000
print("\n" + "="*80)
print("ITEM 3: ALL WRITES TO 0x60000000 & 0x60040000 (FMC FPGA PORTS)")
print("="*80)
fmc_writes = []
for addr, insn in instructions.items():
    if insn.mnemonic.startswith(('str', 'stm')):
        # Check if surrounding instructions load 0x60000000 or 0x60040000
        # Look back 10 instructions
        for back_a in range(max(FLASH_BASE, addr - 32), addr, 2):
            if back_a in instructions:
                bi = instructions[back_a]
                if '0x60000000' in bi.op_str or '0x60040000' in bi.op_str:
                    fmc_writes.append((addr, insn))
                    break

print(f"Found {len(fmc_writes)} FMC write instructions.")
for addr, insn in fmc_writes[:40]:
    print(f"  0x{addr:08X}: {insn.mnemonic:8s} {insn.op_str}")

# 4. ALL XREFS TO LITERAL 48000
print("\n" + "="*80)
print("ITEM 5: ALL REFERENCES TO LITERAL 48000")
print("="*80)
pos = 0
b48k = struct.pack('<I', 48000)
while True:
    pos = app_bin.find(b48k, pos)
    if pos == -1: break
    addr = FLASH_BASE + pos
    print(f"Literal 48000 at Flash Address 0x{addr:08X} (offset 0x{pos:X})")
    # Disassemble around it
    for ctx_a in range(max(FLASH_BASE, addr - 32), min(FLASH_END, addr + 32), 2):
        if ctx_a in instructions:
            ci = instructions[ctx_a]
            print(f"    0x{ci.address:08X}: {ci.mnemonic:8s} {ci.op_str}")
    pos += 4
