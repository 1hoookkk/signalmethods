import capstone
import struct

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)

# In Thumb-2:
# 332 = 0x014C -> can be loaded via:
# 1. movw rX, #0x14c -> f240 1x4c
# 2. ldr rX, [pc, #offset] from literal pool
# 3. multiplication: mul / mla / smull
# Let's search all instructions in the 128KB firmware for any multiplication by 332, or indexing

print("=== SEARCHING FOR CUBE INDEX MULTIPLICATION (x * 332 or x * 320 or x * 44) ===")

for ins in md.disasm(firmware, base_addr):
    # Check if instruction references 332, 0x14c, 320, 0x140, 44, 0x2c
    op = ins.op_str
    if '332' in op or '0x14c' in op or '0x140' in op or '320' in op or '#0x2c' in op or '#44' in op:
        print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")

# Also search for functions that call the 36-byte unpacker 0x080382FC
print("\n=== SEARCHING FOR CALLS TO 0x080382FC ===")
for ins in md.disasm(firmware, base_addr):
    if ins.mnemonic in ('b', 'bl', 'b.w', 'bl.w') and '80382fc' in ins.op_str:
        print(f"0x{ins.address:08X}:  {ins.bytes.hex():12s}  {ins.mnemonic:8s} {ins.op_str}")
