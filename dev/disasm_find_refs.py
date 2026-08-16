import struct, os, re
import capstone

# 1. Load demodulated data
import inspect_vpg1
data = inspect_vpg1.data

# The payload starts at vector table: offset 774
vec_offset = 774
# Read length from header (bytes 5..8 after _VPG1: 749+5 = 754)
payload_len = struct.unpack('<I', data[749+5 : 749+9])[0]
print(f"Payload length from header: {payload_len} bytes (0x{payload_len:X})")

app_bin = data[vec_offset : vec_offset + payload_len]
print(f"Extracted App Binary: {len(app_bin)} bytes")

# Save extracted clean binary
bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'wb') as fp:
    fp.write(app_bin)
print(f"Saved application image to {bin_path}")

FLASH_BASE = 0x08020000

# Let's find strings in the binary and their flash addresses
strings_to_find = [
    "Bad Pulse",
    "Bad Sync Mark",
    "Bad Checksum",
    "Load Cubes",
    "Xform Controls Dist'n",
    "Left Distortion",
    "Right Distortion",
    "Current Cube Revision:",
    "Play Morpheus Cube .wav",
    "Cubes Successfully Loaded",
    "Not Morpheus Cube file",
    "Dave'sRave"
]

print("\n=== STRING LOCATIONS IN FLASH ===")
str_addrs = {}
for s in strings_to_find:
    s_bytes = s.encode('ascii')
    pos = app_bin.find(s_bytes)
    if pos != -1:
        addr = FLASH_BASE + pos
        str_addrs[s] = addr
        print(f"'{s}' -> Flash Addr: 0x{addr:08X} (offset 0x{pos:X})")
    else:
        print(f"'{s}' -> NOT FOUND")

# Initialize Capstone Disassembler for ARM Thumb
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail = True

# Search for literal pool references to these string addresses
print("\n=== CODE REFERENCES TO STRINGS ===")
for s, addr in str_addrs.items():
    addr_bytes = struct.pack('<I', addr)
    # Find literal pool constants
    pos = 0
    while True:
        pos = app_bin.find(addr_bytes, pos)
        if pos == -1: break
        pool_addr = FLASH_BASE + pos
        print(f"Literal pool reference to '{s}' at 0x{pool_addr:08X}")
        
        # Disassemble around this area to find the function calling it
        # Look backwards 128 bytes to find function entry or LDR instructions referencing pool_addr
        start_search = max(0, pos - 256)
        code_chunk = app_bin[start_search : pos + 128]
        chunk_base = FLASH_BASE + start_search
        
        for insn in md.disasm(code_chunk, chunk_base):
            # Check if instruction references pool_addr
            if insn.mnemonic == 'ldr' and len(insn.operands) > 1:
                op = insn.operands[1]
                if op.type == capstone.arm.ARM_OP_MEM:
                    # Check PC-relative target
                    pass
            # Print instructions referencing string
            if f"0x{pool_addr:x}" in insn.op_str.lower() or f"0x{addr:x}" in insn.op_str.lower():
                print(f"  --> 0x{insn.address:08X}: {insn.mnemonic} {insn.op_str}")
        
        pos += 4
