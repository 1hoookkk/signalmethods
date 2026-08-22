import re
path = r"C:\Users\hooki\trench-native\dev\disasm\vulcan_full.asm"
targets = {0x08037830: "poolA", 0x080382C4: "poolB", 0x08037B48: "poolC"}
refs = []
with open(path, encoding="utf-8", errors="replace") as fh:
    for lineno, line in enumerate(fh, 1):
        m = re.match(r"0x([0-9A-Fa-f]{8}):\s+([0-9A-Fa-f]{4,8})\s+([A-Za-z.\-@!]+)\s*(.*)", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        op = m.group(3)
        rest = m.group(4)
        if "pc" not in rest:
            continue
        im = re.search(r"#0x([0-9A-Fa-f]+)", rest)
        if not im:
            continue
        imm = int(im.group(1), 16)
        tgt = ((addr + 4) & ~3) + imm
        if tgt in targets:
            refs.append((addr, targets[tgt], op, rest.strip(), lineno))
for a, t, op, rest, ln in refs:
    print(f"{ln}: 0x{a:08X} {op} {rest}   -> {t}")
