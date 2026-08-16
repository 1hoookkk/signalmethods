import capstone, struct

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Read TBH table at 0x08033D98
tbh_addr = 0x08033D98
tbh_off = tbh_addr - FLASH_BASE

num_entries = 7
offsets = struct.unpack(f'<{num_entries}H', app_bin[tbh_off : tbh_off + num_entries * 2])

print("=== TBH FILTER MODE JUMP TABLE (at 0x08033D98) ===")
targets = []
for i, off in enumerate(offsets):
    target = tbh_addr + (off * 2)
    targets.append((i, target))
    print(f"Mode {i}: Table Offset = 0x{off:04X} -> Target Address = 0x{target:08X}")

print("\n=== DISASSEMBLY OF EACH FILTER MODE HANDLER ===")
for mode_idx, t_addr in targets:
    print(f"\n--- MODE {mode_idx} HANDLER (0x{t_addr:08X}) ---")
    chunk = app_bin[t_addr - FLASH_BASE : t_addr - FLASH_BASE + 64]
    for insn in md.disasm(chunk, t_addr):
        print(f"  0x{insn.address:08X}: {insn.mnemonic:8s} {insn.op_str}")
        if insn.mnemonic in ('b', 'bx', 'pop', 'b.w'):
            if insn.address > t_addr + 8: break
