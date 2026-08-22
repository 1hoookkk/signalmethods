import capstone, struct, re

BASE = 0x080375FC
data = open(r"C:\Users\hooki\trench-native\dev\disasm\vulcan_callback.bin", "rb").read()
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

N = len(data)
lines = []
for ins in md.disasm(data, BASE):
    u = ""
    if "pc" in ins.op_str and "[" in ins.op_str:
        m = re.search(r"\[pc,\s*#(0x[0-9a-fA-F]+|\d+)\]", ins.op_str)
        if m:
            off = int(m.group(1), 16)
            L = (ins.address & ~3) + 4 + off
            if 0 <= L - BASE <= N - 4:
                val = struct.unpack_from("<I", data, L - BASE)[0]
                fl = struct.unpack_from("<f", data, L - BASE)[0]
                u = f"   ; lit@0x{L:08X} = {val:#010x}  f32={fl:.7g}"
    if ins.mnemonic.startswith("bl"):
        m = re.search(r"#(0x[0-9a-fA-F]+)", ins.op_str)
        tgt = (ins.address + 4 + int(m.group(1), 16)) & 0xfffffffe if m else 0  # decoded by capstone in 64-bit mode? no, uses imm text
        u = f"   ; -> 0x{tgt:08X}" if m else u
    lines.append(f"{ins.address:08X}: {ins.mnemonic:8s} {ins.op_str}{u}")

open(r"C:\Users\hooki\trench-native\dev\disasm\callback_annot.txt", "w").write("\n".join(lines))
print(len(lines))
