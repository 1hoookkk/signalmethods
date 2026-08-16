with open('ref/presets/talking_hedz.bin', 'rb') as fp:
    th_bytes = fp.read()

print(f"talking_hedz.bin length: {len(th_bytes)} bytes")
print("Hex dump:")
for row in range(0, len(th_bytes), 16):
    chunk = th_bytes[row:row+16]
    hex_str = ' '.join(f"{b:02X}" for b in chunk)
    ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
    print(f"  {row:04X}: {hex_str:48s}  {ascii_str}")
