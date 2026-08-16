import struct, math, json

STREAM = r'C:\Users\hooki\trench-authoring\ref\morpheus\records_stream.bin'
REC0 = 340
STRIDE = 332
NREC = 289
FS = 39062.5

K_ANGLE = struct.unpack('<f', struct.pack('<I', 0x32C90FDB))[0]
K_RADIUS = struct.unpack('<f', struct.pack('<I', 0x32800800))[0]
K_GAIN = struct.unpack('<f', struct.pack('<I', 0x338007FF))[0]

data = open(STREAM, 'rb').read()

def block_fields(block_bytes, nfields):
    nwords = (len(block_bytes) + 3) // 4
    words = struct.unpack('<%dI' % nwords, block_bytes.ljust(nwords * 4, b'\0'))
    bits = 0
    for w in words:
        bits = (bits << 32) | w
    total = 32 * nwords
    return [(bits >> (total - 11 * (k + 1))) & 0x7FF for k in range(nfields)]

def encoded(field):
    return (field << 4) | (0xF if field else 0)

def decode_angle(field):
    v = encoded(field)
    E = (v >> 11) & 0xF
    M = v & 0x7FF
    return ((M | 0x800) << E) * K_ANGLE

def float_law(v):
    E = (v >> 11) & 0xF
    M = v & 0x7FF
    h = 1 if E else 0
    return (M | (h << 11)) << (E - h)

def decode_radius(field):
    return 1.0 - float_law(encoded(field)) * K_RADIUS

def decode_gain(field):
    return float_law(encoded(field)) * K_GAIN

def decode_record(rec):
    name = rec[:12].decode('ascii', errors='replace').rstrip()
    payload = rec[12:]
    stage_params = []
    for s in range(7):
        f = block_fields(payload[s*44:(s+1)*44], 32)
        stage_params.append([f[p*8:(p+1)*8] for p in range(4)])
    gains = block_fields(payload[308:320], 8)
    tail = struct.unpack_from('<I', payload, 316)[0]
    corners = []
    for c in range(8):
        secs = []
        for s in range(7):
            pf, pr, zf, zr = (stage_params[s][p][c] for p in range(4))
            pa, za = decode_angle(pf), decode_angle(zf)
            secs.append({
                'pole': {'hz': round(pa*FS/(2*math.pi), 2), 'r': round(decode_radius(pr), 6)},
                'zero': {'hz': round(za*FS/(2*math.pi), 2), 'r': round(decode_radius(zr), 6)},
                'raw': [pf, pr, zf, zr],
            })
        corners.append({'gain': round(decode_gain(gains[c]), 6), 'gain_raw': gains[c], 'sections': secs})
    return {'name': name, 'tail_word': tail, 'corners': corners}

def is_null_stage(sec):
    return sec['raw'][0] == sec['raw'][2] and sec['raw'][1] == sec['raw'][3]

def is_null_corner(corner):
    return all(is_null_stage(s) for s in corner['sections'])

if __name__ == '__main__':
    cubes = []
    for i in range(NREC):
        rec = data[REC0 + i*STRIDE : REC0 + (i+1)*STRIDE]
        cubes.append(decode_record(rec))

    names_clean = sum(1 for i in range(NREC)
                      if all(32 <= b <= 126 for b in data[REC0+i*STRIDE : REC0+i*STRIDE+12]))
    print(f"clean names: {names_clean}/289")

    null = cubes[0]
    print("Null Cube fully identity:", all(is_null_corner(c) for c in null['corners']))

    pole_r1 = 0
    zero_r1 = 0
    pole_rmax = 0.0
    active_stages = 0
    total_stages = 0
    for cu in cubes:
        for c in cu['corners']:
            for s in c['sections']:
                total_stages += 1
                if not is_null_stage(s):
                    active_stages += 1
                if s['raw'][1] == 0:
                    pole_r1 += 1
                if s['raw'][3] == 0:
                    zero_r1 += 1
                if s['raw'][1] != 0:
                    pole_rmax = max(pole_rmax, s['pole']['r'])
    print(f"stage states: {total_stages}, non-identity: {active_stages}")
    print(f"pole radius == 1.0 (field 0) count: {pole_r1}  <- must be 0 for stability")
    print(f"zero radius == 1.0 (field 0) count: {zero_r1}  (traveling nulls, legal)")
    print(f"max pole radius (nonzero field): {pole_rmax:.6f}")

    dot4 = [i for i, cu in enumerate(cubes) if cu['name'].endswith('.4')]
    planar = 0
    for i in dot4:
        cs = cubes[i]['corners']
        even_null = all(is_null_corner(cs[c]) for c in (0, 2, 4, 6))
        pairwise = all(
            [s['raw'] for s in cs[c]['sections']] == [s['raw'] for s in cs[c+1]['sections']]
            for c in (0, 2, 4, 6))
        if even_null or pairwise:
            planar += 1
    print(f".4 cubes: {len(dot4)}, structurally planar (even corners null or t-duplicated): {planar}")

    gains = sorted(set(g for cu in cubes for g in (c['gain'] for c in cu['corners'])))
    print(f"distinct corner gains: {len(gains)}; min {gains[0]:.4f} max {gains[-1]:.4f}")

    tails = set(cu['tail_word'] & 0xFF for cu in cubes)
    print(f"tail-word low bytes seen: {sorted(hex(t) for t in tails)}")

    out = {
        'source': 'ref/morpheus/records_stream.bin (biphase-mark transfer audio, 289 records, stride 332, record 0 at byte 340)',
        'law': {
            'record': '12-byte ASCII name + 320-byte payload; payload = 7 stage blocks of 44 bytes + 12-byte gain/tail block',
            'stage_block': '32 contiguous 11-bit fields, MSB-first within little-endian u32 stream: param-major, 8 corners per param; params = [pole_angle, pole_radius, zero_angle, zero_radius]',
            'gain_block': 'payload bytes 308..319: 8 x 11-bit per-corner cascade gains',
            'field': '11-bit stored = top 11 bits of 15-bit runtime code; unpacker refills low 4 bits with 0xF (firmware 0x080382FC)',
            'angle': 'theta = ((M|0x800) << E) * float32(pi/2^27), E=code>>11, M=code&0x7FF (firmware 0x080378D0)',
            'radius': 'R = 1 - ((M | (E!=0)<<11) << (E - (E!=0))) * 0x32800800f  (denormal-aware; field 0 -> R=1.0)',
            'gain': 'g = ((M | (E!=0)<<11) << (E - (E!=0))) * 0x338007FFf (linear; 1787 -> 0.984 ~ unity)',
            'corner_index': 'bit0 = Transform2, bit1 = Morph, bit2 = Frequency tracking (evidence: LPFlange.4 corner structure vs manual F001)',
            'hz_datum': 39062.5,
        },
        'cubes': [
            {
                'index': i,
                'name': cu['name'],
                'tail_word': cu['tail_word'],
                'corners': [
                    {
                        'gain': c['gain'],
                        'sections': [
                            {'pole': s['pole'], 'zero': s['zero'], 'raw': s['raw']}
                            for s in c['sections']
                        ],
                    } for c in cu['corners']
                ],
            } for i, cu in enumerate(cubes)
        ],
    }
    out_path = r'C:\Users\hooki\trench-authoring\ref\morpheus\cubes_decoded.json'
    with open(out_path, 'w') as f:
        json.dump(out, f, indent=1)
    print(f"wrote {out_path}")
