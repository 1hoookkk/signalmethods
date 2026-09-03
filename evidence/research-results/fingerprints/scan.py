import os, re, io, sys, zipfile, json, struct, datetime

SCRATCH = r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\ab3a7d2d-7fe0-4f40-bb35-c2c44b03b04e\scratchpad\fingerprints"
DUMP_PATH = os.path.join(SCRATCH, "string_dump.txt")
REPORT_JSON = os.path.join(SCRATCH, "findings.json")
MAX_FILE_MB = 200
MAX_ZIP_DEPTH = 4

ASCII_RE = re.compile(rb'[\x20-\x7e]{6,}')
UTF16_RE = re.compile(rb'(?:[\x20-\x7e]\x00){3,}')

PATH_RE = re.compile(
    rb'([A-Za-z]:\\[^\x00-\x1f"<>|]{3,200}|/(?:usr|home|people)/[^\x00-\x1f"<>|]{3,200}|'
    rb'[A-Za-z0-9_\-]{1,80}\.(?:c|h|cpp|pas|asm|pdb|mak|lib|obj|dsp|dsw|prj)\b)',
    re.IGNORECASE)
MAX_CLASSIFY_CHARS = 3000

TOOLWORDS = [
    "FILTER","CUBE","CORNER","MORPH","FRAME","ZPLANE","Z-PLANE","HCHIP","H-CHIP",
    "GCHIP","DSPX","CUBEEDIT","FILTEDIT","FILTDEF","IMPORT","LEGACY","DESIGN",
    "COMPILE","AUTHOR","EDITOR","WORKSTATION","SGI","IRIX","MIPS","MATLAB",
    "HYPERSIGNAL","PROTEUS","MORPHEUS","ULTRAPROTEUS","AUDITY","EMULATOR",
]
TOOLWORD_RE = re.compile("(" + "|".join(re.escape(w) for w in TOOLWORDS) + ")", re.IGNORECASE)
EXT_TOOLWORD_RE = re.compile(r'\b[\w\-]+\.(flt|fil|fdf|cub|cube|zpf|mrf|prm)\b', re.IGNORECASE)

COMPILER_WORDS = [
    "RSDS","Microsoft","Borland","Metrowerks","CodeWarrior","MPW","THINK C","gcc","MIPSpro",
]
COMPILER_RE = re.compile("(" + "|".join(re.escape(w) for w in COMPILER_WORDS) + ")")
COPYRIGHT_RE = re.compile(r'(?:Copyright|\(C\)|\xa9)\D{0,10}(19[89]\d|20[0-3]\d)', re.IGNORECASE)

SKIP_PATH_FRAGMENTS = [
    "irix53.chd",
    os.path.join("runtime", "mame-0.289").lower(),
]

MEDIA_SKIP_EXTS = {
    ".mp4",".webm",".mov",".avi",".mkv",".mp3",".wav",".flac",".ogg",
    ".png",".jpg",".jpeg",".gif",".bmp",".ico",".tif",".tiff",".webp",
    ".ttf",".otf",".woff",".woff2",
}
MAX_MATCHES_PER_FILE = 20000

results = []
counts = {"files_scanned":0, "path_strings":0, "toolwords":0, "compiler_pdb":0, "record_candidates":0}
dump_f = open(DUMP_PATH, "w", encoding="utf-8", errors="replace")

def log_dump(header):
    dump_f.write("\n" + "="*100 + "\n" + header + "\n" + "="*100 + "\n")

def should_skip(path):
    lp = path.lower()
    for frag in SKIP_PATH_FRAGMENTS:
        if frag in lp:
            return True
    return False

def extract_strings(data, source_label):
    findings = {"path": [], "tool": [], "compiler": []}
    n = 0
    for m in ASCII_RE.finditer(data):
        n += 1
        if n > MAX_MATCHES_PER_FILE:
            findings["_truncated_ascii"] = True
            break
        s = m.group()
        off = m.start()
        try:
            txt = s.decode('ascii')
        except Exception:
            continue
        classify(txt, off, findings)
    n = 0
    for m in UTF16_RE.finditer(data):
        n += 1
        if n > MAX_MATCHES_PER_FILE:
            findings["_truncated_utf16"] = True
            break
        raw = m.group()
        try:
            txt = raw.decode('utf-16-le', errors='ignore')
        except Exception:
            continue
        off = m.start()
        classify(txt, off, findings, utf16=True)
    return findings

def classify(txt, off, findings, utf16=False):
    tag = "u16" if utf16 else "a"
    full_len = len(txt)
    if full_len > MAX_CLASSIFY_CHARS:
        txt = txt[:MAX_CLASSIFY_CHARS]
    if PATH_RE.search(txt.encode('ascii', 'ignore')):
        findings["path"].append((off, tag, txt[:160]))
    for mm in TOOLWORD_RE.finditer(txt):
        start = max(0, mm.start()-20)
        end = min(len(txt), mm.end()+20)
        findings["tool"].append((off, tag, txt[start:end], mm.group()))
    for mm in EXT_TOOLWORD_RE.finditer(txt):
        start = max(0, mm.start()-20)
        end = min(len(txt), mm.end()+20)
        findings["tool"].append((off, tag, txt[start:end], mm.group()))
    if COMPILER_RE.search(txt):
        findings["compiler"].append((off, tag, txt[:160]))
    if COPYRIGHT_RE.search(txt):
        findings["compiler"].append((off, tag, txt[:160]))

def record_size_scan(data, source_label):
    candidates = []
    for size in (240, 30, 5, 60, 120, 480, 20, 40):
        if len(data) < size*4 or len(data) % size != 0:
            continue
        n = len(data) // size
        if n < 3 or n > 20000:
            continue
        chunk0 = data[:size]
        chunk1 = data[size:size*2]
        if chunk0 == chunk1:
            continue
        nonzero0 = sum(1 for b in chunk0 if b != 0)
        if nonzero0 < size * 0.2:
            continue
        candidates.append((size, n))
    if candidates:
        counts["record_candidates"] += 1
        results.append({
            "kind": "record_candidate",
            "file": source_label,
            "size_bytes": len(data),
            "candidates": candidates,
        })

import time as _time

def process_blob(data, label, depth=0):
    counts["files_scanned"] += 1
    if counts["files_scanned"] % 25 == 0:
        print(f"...scanned {counts['files_scanned']} files, last={label}", flush=True)
    _t0 = _time.time()
    _big = len(data) > 3*1024*1024
    if _big:
        print(f"  [big file] {label} ({len(data)} bytes) - scanning...", flush=True)
    if len(data) > MAX_FILE_MB*1024*1024:
        data = data[:MAX_FILE_MB*1024*1024]
    findings = extract_strings(data, label)
    if findings["path"] or findings["tool"] or findings["compiler"]:
        log_dump(f"FILE: {label}  size={len(data)}")
        if findings["path"]:
            dump_f.write(f"-- path-shaped strings ({len(findings['path'])}) --\n")
            for off, tag, txt in findings["path"][:500]:
                dump_f.write(f"  [{tag} off={off}] {txt}\n")
        if findings["tool"]:
            dump_f.write(f"-- tool-ish words ({len(findings['tool'])}) --\n")
            for off, tag, ctx, word in findings["tool"][:500]:
                dump_f.write(f"  [{tag} off={off}] match={word!r} ctx={ctx!r}\n")
        if findings["compiler"]:
            dump_f.write(f"-- compiler/pdb/copyright ({len(findings['compiler'])}) --\n")
            for off, tag, txt in findings["compiler"][:200]:
                dump_f.write(f"  [{tag} off={off}] {txt}\n")
        dump_f.flush()
    counts["path_strings"] += len(findings["path"])
    counts["toolwords"] += len(findings["tool"])
    counts["compiler_pdb"] += len(findings["compiler"])
    if findings["path"] or findings["tool"] or findings["compiler"]:
        results.append({
            "kind": "strings",
            "file": label,
            "size": len(data),
            "n_path": len(findings["path"]),
            "n_tool": len(findings["tool"]),
            "n_compiler": len(findings["compiler"]),
            "top_tool": findings["tool"][:15],
            "top_path": findings["path"][:15],
            "top_compiler": findings["compiler"][:15],
        })
    ext = os.path.splitext(label)[1].lower()
    if ext in (".bin", ".rom", ".body", ".dat") or "bitstream" in label.lower():
        record_size_scan(data, label)
    if _big:
        print(f"  [big file done] {label} took {_time.time()-_t0:.1f}s", flush=True)
    if ext == ".zip" and depth < MAX_ZIP_DEPTH:
        try:
            zf = zipfile.ZipFile(io.BytesIO(data))
            for info in zf.infolist():
                if info.is_dir():
                    continue
                if info.file_size > MAX_FILE_MB*1024*1024:
                    continue
                if should_skip(info.filename):
                    continue
                try:
                    member_data = zf.read(info)
                except Exception as e:
                    continue
                process_blob(member_data, f"{label}!{info.filename}", depth+1)
        except Exception as e:
            dump_f.write(f"\n[zip open error] {label}: {e}\n")

def process_pdf(path, label):
    counts["files_scanned"] += 1
    try:
        import fitz
        doc = fitz.open(path)
        text_parts = []
        for page in doc:
            text_parts.append(page.get_text())
        text = "\n".join(text_parts)
    except Exception as e:
        dump_f.write(f"\n[pdf error] {label}: {e}\n")
        return
    data = text.encode('utf-8', errors='ignore')
    findings = {"path": [], "tool": [], "compiler": []}
    for line_no, line in enumerate(text.splitlines()):
        for mm in TOOLWORD_RE.finditer(line):
            start = max(0, mm.start()-30)
            end = min(len(line), mm.end()+30)
            findings["tool"].append((line_no, "pdf", line[start:end], mm.group()))
        for mm in EXT_TOOLWORD_RE.finditer(line):
            start = max(0, mm.start()-30)
            end = min(len(line), mm.end()+30)
            findings["tool"].append((line_no, "pdf", line[start:end], mm.group()))
        if PATH_RE.search(line.encode('ascii','ignore')):
            findings["path"].append((line_no, "pdf", line[:160]))
        if COMPILER_RE.search(line) or COPYRIGHT_RE.search(line):
            findings["compiler"].append((line_no, "pdf", line[:160]))
    counts["path_strings"] += len(findings["path"])
    counts["toolwords"] += len(findings["tool"])
    counts["compiler_pdb"] += len(findings["compiler"])
    if findings["path"] or findings["tool"] or findings["compiler"]:
        log_dump(f"PDF TEXT: {label}  chars={len(text)}")
        if findings["tool"]:
            dump_f.write(f"-- tool-ish words ({len(findings['tool'])}) --\n")
            for line_no, tag, ctx, word in findings["tool"][:300]:
                dump_f.write(f"  [line {line_no}] match={word!r} ctx={ctx!r}\n")
        if findings["path"]:
            dump_f.write(f"-- path-shaped ({len(findings['path'])}) --\n")
            for line_no, tag, txt in findings["path"][:200]:
                dump_f.write(f"  [line {line_no}] {txt}\n")
        if findings["compiler"]:
            dump_f.write(f"-- compiler/copyright ({len(findings['compiler'])}) --\n")
            for line_no, tag, txt in findings["compiler"][:100]:
                dump_f.write(f"  [line {line_no}] {txt}\n")
        results.append({
            "kind": "pdf",
            "file": label,
            "n_path": len(findings["path"]),
            "n_tool": len(findings["tool"]),
            "n_compiler": len(findings["compiler"]),
            "top_tool": findings["tool"][:15],
        })

def process_pe_exports(path, label):
    try:
        import pefile
    except ImportError:
        return
    _t0 = _time.time()
    try:
        pe = pefile.PE(path, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
    except Exception as e:
        print(f"  [pe_exports error] {label}: {e}", flush=True)
        return
    dt = _time.time() - _t0
    if dt > 2:
        print(f"  [pe_exports] {label} took {dt:.1f}s", flush=True)
    exports = []
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if exp.name:
                exports.append(exp.name.decode('utf-8', errors='replace'))
    keywords = ("Filter","Morph","Cube","Frame","Coef","Zplane","ZPlane","zplane")
    matched = [e for e in exports if any(k.lower() in e.lower() for k in keywords)]
    is_pe = True
    machine = hex(pe.FILE_HEADER.Machine)
    timestamp = pe.FILE_HEADER.TimeDateStamp
    try:
        ts_str = datetime.datetime.utcfromtimestamp(timestamp).isoformat()
    except Exception:
        ts_str = str(timestamp)
    results.append({
        "kind": "pe_exports",
        "file": label,
        "n_exports": len(exports),
        "matched_exports": matched,
        "all_exports_sample": exports[:40],
        "machine": machine,
        "link_timestamp": ts_str,
    })
    log_dump(f"PE EXPORTS: {label}  n_exports={len(exports)} link_time={ts_str} machine={machine}")
    if matched:
        dump_f.write(f"-- MATCHED exports ({len(matched)}) --\n")
        for e in matched:
            dump_f.write(f"  {e}\n")
    elif exports:
        dump_f.write(f"-- sample exports (no keyword match), first 20 --\n")
        for e in exports[:20]:
            dump_f.write(f"  {e}\n")

def walk_path(root):
    if os.path.isfile(root):
        handle_file(root)
        return
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not should_skip(os.path.join(dirpath, d))]
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            if should_skip(full):
                continue
            handle_file(full)

def handle_file(full):
    try:
        size = os.path.getsize(full)
    except OSError:
        return
    if size == 0:
        return
    if size > MAX_FILE_MB*1024*1024:
        dump_f.write(f"\n[skipped, too large {size} bytes]: {full}\n")
        dump_f.flush()
        return
    ext = os.path.splitext(full)[1].lower()
    if ext in MEDIA_SKIP_EXTS:
        dump_f.write(f"\n[skipped media ext]: {full} ({size} bytes)\n")
        dump_f.flush()
        return
    label = full
    if ext == ".pdf":
        process_pdf(full, label)
        return
    try:
        with open(full, "rb") as f:
            data = f.read()
    except Exception as e:
        dump_f.write(f"\n[read error] {full}: {e}\n")
        return
    process_blob(data, label, depth=0)
    if ext in (".exe", ".dll", ".ex_", ".sys", ".ocx"):
        process_pe_exports(full, label)
    if data[:4] == b'MZ\x90\x00' or data[:2] == b'MZ':
        if ext not in (".exe", ".dll", ".ex_", ".sys", ".ocx"):
            process_pe_exports(full, label)

TARGETS = [
    r"C:\Users\hooki\Downloads\EMU - ESI-4000_v210.zip",
    r"C:\Users\hooki\Downloads\EMU - ESI-32_OSv2.10.zip",
    r"C:\Users\hooki\Downloads\EMU - ESI-4000_3.02.zip",
    r"C:\Users\hooki\Downloads\EMU-ESI-32.zip",
    r"C:\Users\hooki\Downloads\Emu Emulator Boot ROMs (various, includes rare WILDCARD).zip",
    r"C:\Users\hooki\Downloads\Emulator 2+ not signed IC28 2764.zip",
    r"C:\Users\hooki\Downloads\emu_re_artifacts",
    r"C:\Users\hooki\Downloads\e-mu_eos_technical_documents.pdf",
    r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\ab3a7d2d-7fe0-4f40-bb35-c2c44b03b04e\scratchpad\fingerprints\proteusx_unpacked",
    r"C:\Users\hooki\trench-native\evidence\research-results\disassembly\eos410_stage_builders.asm",
    r"C:\Users\hooki\trench-native\evidence\research-results\emu-sound-department-lineage",
    r"C:\Users\hooki\trench-native\evidence\research-results\emu-sgi-1993",
    r"C:\Users\hooki\trench-native\evidence\factory-data",
    r"C:\Program Files (x86)\E-MU\Emulator X3",
]

for t in TARGETS:
    print(f"=== TARGET: {t} ===", flush=True)
    if not os.path.exists(t):
        dump_f.write(f"\n[MISSING TARGET] {t}\n")
        dump_f.flush()
        print("  MISSING", flush=True)
        continue
    walk_path(t)
    dump_f.flush()
    print(f"  done, files_scanned so far={counts['files_scanned']}", flush=True)

dump_f.close()

with open(REPORT_JSON, "w", encoding="utf-8") as jf:
    json.dump({"counts": counts, "results": results}, jf, indent=1, default=str)

print("DONE")
print("counts:", counts)
print("dump:", DUMP_PATH)
print("json:", REPORT_JSON)
