import numpy as np

# Load recovered bitstream
bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
stride = 332
n_records = 289

# Extract all 289 records (12-byte name + 320-byte payload)
names = []
payloads = []
for i in range(n_records):
    start = rec0_offset + i * stride
    rec = data[start : start + stride]
    names.append(rec[:12].decode('ascii', errors='replace'))
    payloads.append(np.frombuffer(rec[12:], dtype=np.uint8))

payloads = np.array(payloads) # Shape: (289, 320)
print(f"Loaded {n_records} payloads of shape {payloads.shape}")

# 1. Autocorrelation along the 320-byte payload axis
# Measure mean autocorrelation across all 289 records for lags 1..160
lags = np.arange(1, 161)
autocorr = np.zeros(len(lags))

for p in payloads:
    p_norm = p.astype(float) - np.mean(p)
    var = np.var(p_norm)
    if var > 0:
        for li, lag in enumerate(lags):
            corr = np.mean(p_norm[:-lag] * p_norm[lag:]) / var
            autocorr[li] += corr

autocorr /= n_records

print("\n=== TOP PERIODIC LAGS IN 320-BYTE PAYLOAD ===")
top_lags = np.argsort(autocorr)[::-1][:15]
for li in top_lags:
    print(f"Lag {lags[li]:3d} bytes: Autocorrelation = {autocorr[li]:+.4f}")

# 2. Byte Variance Profile across 320 positions (column-wise variance)
variances = np.var(payloads, axis=0)
means = np.mean(payloads, axis=0)

# Check if there are fixed / constant header bytes at specific offsets
const_cols = np.where(variances < 1.0)[0]
print(f"\nNear-constant byte positions across 289 records (var < 1.0): {len(const_cols)}")
for c in const_cols[:20]:
    print(f"  Offset +{c:3d}: mean={means[c]:.2f}, var={variances[c]:.4f}, values={np.unique(payloads[:, c])}")

# 3. Test Block Divisions:
# Division A: 8 blocks of 40 bytes
# Division B: 7 blocks of 44 bytes (= 308 bytes + 12B header)
# Division C: 6 blocks of 50 bytes
# Division D: 4 blocks of 80 bytes
for b_size in [36, 40, 44, 48, 50, 60, 64, 80]:
    n_blocks = 320 // b_size
    rem = 320 % b_size
    # Compute cross-correlation between block 0 and block 1 across all records
    b_corrs = []
    for p in payloads:
        b0 = p[:b_size].astype(float)
        b1 = p[b_size:2*b_size].astype(float)
        if np.std(b0) > 0 and np.std(b1) > 0:
            b_corrs.append(np.corrcoef(b0, b1)[0, 1])
    mean_corr = np.mean(b_corrs) if b_corrs else 0.0
    print(f"Block size {b_size:2d} bytes ({n_blocks} full blocks, {rem} rem): Mean adjacent block correlation = {mean_corr:+.4f}")
