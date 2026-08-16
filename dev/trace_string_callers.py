import capstone, struct, os

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Target string addresses
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

print("Scanning for all references to target strings via PC-relative LDR / MOVW+MOVT / Literal Pools...")

# Method 1: Scan literal pools (32-bit words anywhere in text/rodata)
literal_pools = {}
for i in range(0, len(app_bin) - 4, 4):
    val = struct.unpack('<I', app_bin[i:i+4])[0]
    if val in target_strings:
        pool_addr = FLASH_BASE + i
        literal_pools[pool_addr] = (val, target_strings[val])
        print(f"Literal Pool at 0x{pool_addr:08X} -> points to 0x{val:08X} (\"{target_strings[val]}\")")

# Method 2: Disassemble whole binary and find instructions referencing either target string directly or literal pool
print("\nDisassembling entire binary...")
disasm_lines = list(md.disasm(app_bin, FLASH_BASE))
print(f"Total instructions disassembled: {len(disasm_lines)}")

# Find instructions referencing literal pools
for insn in disasm_lines:
    # Check PC-relative LDR
    if insn.mnemonic == 'ldr':
        # Check operands
        for op in insn.operands:
            if op.type == capstone.arm.ARM_OP_MEM and op.mem.base == capstone.arm.ARM_REG_PC:
                # In ARM Thumb, PC is (address + 4) & ~3 for word alignment or address + 4
                target_mem = ((insn.address + 4) & ~3) + op.mem.disp
                if target_mem in literal_pools:
                    str_addr, str_name = literal_pools[target_mem]
                    print(f"\n[HIT] Function instruction at 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")
                    print(f"      Loads pointer to string: \"{str_name}\" (from pool 0x{target_mem:08X})")
                    
                    # Disassemble function window (from 64 bytes before to 128 bytes after)
                    fn_start = max(FLASH_BASE, insn.address - 64)
                    fn_end = min(FLASH_BASE + len(app_bin), insn.address + 128)
                    chunk = app_bin[fn_start - FLASH_BASE : fn_end - FLASH_BASE]
                    print("      Context Disassembly:")
                    for ctx_insn in md.disasm(chunk, fn_start):
                        marker = "===>" if ctx_insn.address == insn.address else "    "
                        print(f"      {marker} 0x{ctx_insn.address:08X}: {ctx_insn.mnemonic:8s} {ctx_insn.op_str}")
