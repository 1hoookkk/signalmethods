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

print(f"Disassembled {len(instructions)} instructions.")

# Build XREF table for all calls (bl, blx, b, b.w)
xrefs_to = {} # target_addr -> list of (caller_addr, mnemonic, op_str)
for addr, insn in instructions.items():
    if insn.mnemonic in ('bl', 'blx', 'b', 'b.w', 'beq', 'bne', 'bgt', 'blt', 'bge', 'ble', 'bhi', 'blo', 'bcs', 'bcc', 'bmi', 'bpl', 'cbz', 'cbnz'):
        for op in insn.operands:
            if op.type == capstone.arm.ARM_OP_IMM:
                target = op.imm
                if target not in xrefs_to:
                    xrefs_to[target] = []
                xrefs_to[target].append((addr, insn.mnemonic, insn.op_str))

print("=== TASK 1: DISASSEMBLY & CONTEXT FOR 0x08038E00..0x08039150 ===")
# Find function start: look backwards for push {..., lr}
fn_start = 0x08038E00
for test_addr in range(0x08038F68, 0x08038000, -2):
    if test_addr in instructions:
        insn = instructions[test_addr]
        if insn.mnemonic.startswith('push') and 'lr' in insn.op_str:
            fn_start = test_addr
            break

# Find function end: look forwards for pop {..., pc} or bx lr
fn_end = 0x08039150
for test_addr in range(0x08038F68, 0x08039500, 2):
    if test_addr in instructions:
        insn = instructions[test_addr]
        if (insn.mnemonic.startswith('pop') and 'pc' in insn.op_str) or insn.mnemonic == 'bx lr':
            fn_end = test_addr + insn.size
            break

print(f"Function bounds: 0x{fn_start:08X} to 0x{fn_end:08X}")
print(f"Callers (XREFs to 0x{fn_start:08X}):")
for caller, mnem, op in xrefs_to.get(fn_start, []):
    print(f"  0x{caller:08X}: {mnem} {op}")

print("\nFull Instruction Stream:")
for a in range(fn_start, fn_end, 2):
    if a in instructions:
        i = instructions[a]
        # Check PC-relative literal loads
        extra = ""
        if i.mnemonic.startswith(('ldr', 'vldr')) and len(i.operands) > 1:
            op = i.operands[1]
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                target_mem = ((i.address + 4) & ~3) + op.mem.disp
                if FLASH_BASE <= target_mem < FLASH_END - 4:
                    val = struct.unpack('<I', app_bin[target_mem - FLASH_BASE : target_mem - FLASH_BASE + 4])[0]
                    extra = f" // [0x{target_mem:08X}] = 0x{val:08X}"
        print(f"0x{i.address:08X}:  {i.bytes.hex():10s}  {i.mnemonic:8s} {i.op_str}{extra}")
