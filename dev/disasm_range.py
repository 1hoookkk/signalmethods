import capstone, struct, re, sys

BASE = int(sys.argv[1], 16)
END = int(sys.argv[2], 16)
OUT = sys.argv[3]
data = open(r"C:\Users\hooki\trench-native\dev\disasm\vulcan_app_08020000.bin", "rb").read()
start = BASE - 0x08020000
chunk = data[start : (END - 0x08020000)]
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
N = len(chunk)
lines = []
for ins in md.disasm(chunk, BASE):
    u = ""
    if "pc" in ins.op_str and "[" in ins.op_str:
        m = re.search(r"\[pc,\s*#(0x[0-9a-fA-F]+|\d+)\]", ins.op_str)
        if m:
            off = int(m.group(1), 16)
            L = (ins.address & ~3) + 4 + off
            if 0 <= L - BASE <= N - 4:
                val = struct.unpack_from("<I", chunk, L - BASE)[0]
                fl = struct.unpack_from("<f", chunk, L - BASE)[0]
                u = f"; lit 0x{L:08X} = {val:#010x} ({fl:.7g})"
    lines.append(f"{ins.address:08X}: {ins.mnemonic:8s} {ins.op_str} {u}")
open(OUT, "w").write("\n".join(lines))
print(len(lines), "ins ->", OUT)
