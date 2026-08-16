import capstone

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

print("=== TRACING ALL BRANCH / CALL TARGETS IN FIRMWARE ===")
callers_of_dsp = []
callers_of_unpacker = []

for ins in md.disasm(firmware, base_addr):
    # Check if instruction is a branch or call
    if ins.mnemonic.startswith('b') or ins.mnemonic.startswith('bl'):
        # Parse target address from operands
        for op in ins.operands:
            if op.type == capstone.arm.ARM_OP_IMM:
                target = op.imm
                if 0x08037880 <= target <= 0x08037900:
                    callers_of_dsp.append((ins.address, target, ins.mnemonic))
                if 0x080382D0 <= target <= 0x08038330:
                    callers_of_unpacker.append((ins.address, target, ins.mnemonic))

print(f"Callers of DSP coefficient builder (0x080378D0):")
for addr, tgt, mnem in callers_of_dsp:
    print(f"  0x{addr:08X}: {mnem} 0x{tgt:08X}")

print(f"Callers of Unpacker (0x080382FC):")
for addr, tgt, mnem in callers_of_unpacker:
    print(f"  0x{addr:08X}: {mnem} 0x{tgt:08X}")

# Disassemble around callers
for addr, tgt, mnem in callers_of_dsp + callers_of_unpacker:
    print(f"\n--- Disassembly around caller at 0x{addr:08X} ---")
    start_dis = addr - 32
    offset = start_dis - base_addr
    code = firmware[offset : offset + 80]
    for ins in md.disasm(code, start_dis):
        marker = "===>" if ins.address == addr else "    "
        print(f"{marker} 0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")
