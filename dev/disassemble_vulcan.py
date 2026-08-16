import capstone, struct, os, re

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
FLASH_END = FLASH_BASE + len(app_bin)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

print(f"Loaded binary: {len(app_bin)} bytes (0x{FLASH_BASE:08X}..0x{FLASH_END:08X})")

# 1. Target String Addresses
target_strings = {
    0x0803E3F4: "Bad Pulse",
    0x0803E400: "Bad Sync Mark",
    0x0803E420: "Bad Checksum",
    0x0803DC88: "Load Cubes",
    0x0803DBEC: "Xform Controls Dist'n",
    0x0803DBC4: "Left Distortion",
    0x0803DBD8: "Right Distortion",
    0x0803E4E0: "Current Cube Revision:",
    0x0803E4F8: "Play Morpheus Cube .wav",
    0x0803E544: "Cubes Successfully Loaded",
    0x0803E560: "Not Morpheus Cube file"
}

# 2. Linear Sweep Disassembly with Error Recovery
print("\nPerforming Linear Sweep Disassembly across 127KB...")

instructions = {}
pc = 0
while pc < len(app_bin) - 2:
    addr = FLASH_BASE + pc
    try:
        # Try disassembling from current offset
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

print(f"Disassembled {len(instructions)} valid instructions.")

# 3. Detect Literal Pools & String References
print("\n=== SCANNING FOR STRING USAGE & CALLERS ===")
for addr, insn in instructions.items():
    # Check PC-relative LDR
    if insn.mnemonic == 'ldr':
        for op in insn.operands:
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                target_mem = ((insn.address + 4) & ~3) + op.mem.disp
                if FLASH_BASE <= target_mem < FLASH_END - 4:
                    mem_offset = target_mem - FLASH_BASE
                    val = struct.unpack('<I', app_bin[mem_offset : mem_offset + 4])[0]
                    if val in target_strings:
                        print(f"\n[HIT] 0x{insn.address:08X}: {insn.mnemonic:8s} {insn.op_str}")
                        print(f"      Target: \"{target_strings[val]}\" (0x{val:08X}) via pool 0x{target_mem:08X}")
                        
                        # Print function disassembly context (-32 to +48 bytes)
                        ctx_start = max(FLASH_BASE, insn.address - 32)
                        ctx_end = min(FLASH_END, insn.address + 64)
                        for c_addr in range(ctx_start, ctx_end, 2):
                            if c_addr in instructions:
                                ci = instructions[c_addr]
                                m = "===>" if ci.address == insn.address else "    "
                                print(f"      {m} 0x{ci.address:08X}: {ci.mnemonic:8s} {ci.op_str}")

# Save full disassembly to file for deep inspection
print("\nExporting full disassembly to dev/disasm/vulcan_full.asm ...")
with open('dev/disasm/vulcan_full.asm', 'w', encoding='utf-8') as fp:
    for addr in sorted(instructions.keys()):
        i = instructions[addr]
        fp.write(f"0x{addr:08X}:  {i.bytes.hex():10s}  {i.mnemonic:8s} {i.op_str}\n")

print("Export complete.")
