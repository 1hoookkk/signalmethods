import numpy as np

# Let's decode the 16-bit minifloats:
# In Morpheus 16-bit minifloat:
# word w:
# exponent E = (w >> 11) & 0xF (or w >> 12)
# mantissa M = w & 0x7FF (or w & 0xFFF)
# Let's test standard Morpheus minifloat decode functions from stage_law.rs

def decode_minifloat_stage_law(w):
    # trench-core stage_law decode:
    # 11-bit mantissa with implied 12th bit:
    # val = (M * 2^E) * 2^-26 (for damping/radius) or (M * 2^E) * pi/2^27 (for angle)
    exp = (w >> 11) & 0xF
    mant = (w & 0x7FF) | 0x800
    if w == 0:
        return 0.0
    return float(mant * (1 << exp))

bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
c0_payload = data[rec0_offset + 12 : rec0_offset + 320]

print("=== TESTING 5-WORD MINIFLOAT DECODE PER CORNER IN STAGES 1..6 ===")

for s_idx in range(6):
    s_bytes = c0_payload[s_idx*44 : (s_idx+1)*44]
    
    # 20 bytes Part 1 = 10 uint16 words (Corner 0: words 0..4, Corner 1: words 5..9)
    # 4 bytes Middle = Stage pointer / routing
    # 20 bytes Part 2 = 10 uint16 words (Corner 2: words 0..4, Corner 3: words 5..9)
    
    words_p1 = np.frombuffer(s_bytes[:20], dtype='<u2')
    ptr = np.frombuffer(s_bytes[20:24], dtype='<u2')
    words_p2 = np.frombuffer(s_bytes[24:], dtype='<u2')
    
    c0_words = words_p1[:5]
    c1_words = words_p1[5:]
    c2_words = words_p2[:5]
    c3_words = words_p2[5:]
    
    print(f"\n--- Stage S{s_idx+1} (Ptr: 0x{ptr[0]:04x} 0x{ptr[1]:04x}) ---")
    print(f"  Corner 0 (w0..w4): {' '.join(f'0x{w:04x}' for w in c0_words)}")
    print(f"  Corner 1 (w0..w4): {' '.join(f'0x{w:04x}' for w in c1_words)}")
    print(f"  Corner 2 (w0..w4): {' '.join(f'0x{w:04x}' for w in c2_words)}")
    print(f"  Corner 3 (w0..w4): {' '.join(f'0x{w:04x}' for w in c3_words)}")
