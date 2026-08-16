import struct
import numpy as np

# Load recovered bitstream
bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
stride = 332

def unpack_stage_44b(block_44b, stage_idx):
    """
    Exact transcription of STM32 ARM Cortex-M4 assembly at 0x080382FC..0x08038630
    Input: block_44b (44 bytes = 11 uint32 words)
    Output: 16 unpacked 32-bit fields per stage (written to [r1, #0]..[r1, #0x3C])
    """
    u32 = struct.unpack('<11I', block_44b)
    
    # In C/Python, let's trace every field unpacked:
    fields = [0] * 16
    
    # --- Field 0 ([r1, #0]) ---
    r5 = u32[0]
    # 0x08038304: ldr r3, =0x100000; ands r3, (r5 >> 1); beq -> 0 else 0xf0000
    r3 = 0xF0000 if ((r5 >> 1) & 0x100000) else 0
    # 0x0803831A: and r2, 0x7ff0, (r5 >> 6)
    r2 = (r5 >> 6) & 0x7FF0
    r3 += r2
    # 0x08038320: cbz r2 -> 0 else 0xf
    r3 += 0xF if r2 != 0 else 0
    fields[0] = r3
    
    # --- Field 1 ([r1, #4]) ---
    # 0x0803832C: ldr r5, [r0] (u32[0]), ldr r6, [r0, #4] (u32[1])
    r6 = u32[1]
    # (r5 << 21) & 0x100000 ?
    # 0x08038330: and.w r5, r3, r5, lsl #21
    # 0x08038338: and r3, (r6 >> 11), #0x100000
    # adds r3, r3, r5
    term1 = ((u32[0] << 21) & 0x100000)
    term2 = ((r6 >> 11) & 0x100000)
    r3 = term1 + term2
    r3_base = 0xF0000 if r3 != 0 else 0
    r2 = (r6 >> 16) & 0x7FF0
    r3 = r3_base + r2 + (0xF if r2 != 0 else 0)
    fields[1] = r3

    # --- Field 2 ([r1, #8]) ---
    # 0x08038360: ldr r5, [r0, #4] (u32[1]), ldr r2, [r0, #8] (u32[2])
    # ands r3, =0x100000, r5 lsl #11
    r3_flag = ((u32[1] << 11) & 0x100000)
    r3_base = 0xF0000 if r3_flag != 0 else 0
    r5_part = (u32[1] << 6) & 0x7FC0
    r2_part = ((u32[2] >> 26) & 0x30)
    r2 = r5_part + r2_part
    r3 = r3_base + r2 + (0xF if r2 != 0 else 0)
    fields[2] = r3

    # --- Field 3 ([r1, #12]) ---
    # 0x08038398: ldr r5, [r0, #8] (u32[2])
    r3_flag = ((u32[2] << 1) & 0x100000)
    r3_base = 0xF0000 if r3_flag != 0 else 0
    r2 = (u32[2] >> 4) & 0x7FF0
    r3 = r3_base + r2 + (0xF if r2 != 0 else 0)
    fields[3] = r3

    # Return decoded fields
    return fields

# Let's run on Null Cube (Cube 0) Stage 1..7
cube0_bytes = data[rec0_offset + 12 : rec0_offset + 320]
print("=== DECODING CUBE 0 (NULL CUBE) WITH EXACT FIRMWARE UNPACKER (0x080382FC) ===")
for si in range(7):
    stage_44b = cube0_bytes[si*44 : (si+1)*44]
    f = unpack_stage_44b(stage_44b, si)
    print(f"Stage S{si+1} fields 0..3: {[hex(x) for x in f[:4]]}")
