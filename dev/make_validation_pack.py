import os, zipfile, glob, json, math

print("Rebuilding comprehensive validation pack...")

zip_path = "validation_pack.zip"
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    # 1. Raw Binary Bodies (Priority 1)
    z.write("plots/inspector/P2k_010_ooh_to_eee.body240", "P2k_010_ooh_to_eee.body240")
    z.write("plots/inspector/P2k_021_ubu_orator.body240", "P2k_021_ubu_orator.body240")
    
    # 2. Standalone Decoder & README (Priority 2)
    z.write("dev/standalone_body240_decoder.py", "standalone_body240_decoder.py")
    z.write("dev/README_VALIDATION.md", "README.md")
    
    # 3. Authoritative Rust DSP Source Files (Priority 2 & 4)
    z.write("trench-core/src/minifloat.rs", "trench-core/minifloat.rs")
    z.write("trench-core/src/stage_law.rs", "trench-core/stage_law.rs")
    z.write("trench-core/src/response.rs", "trench-core/response.rs")
    
    # 4. Decoded JSON Architectures (Priority 3)
    for p in glob.glob("recipes/architectures/*.json"):
        z.write(p, os.path.join("recipes/architectures", os.path.basename(p)))
        
    # 5. Root-Set & Transposition Mining Scripts (Priority 5 & 6)
    z.write("dev/mine_root_multisets.py", "dev/mine_root_multisets.py")
    z.write("dev/mine_transpositions.py", "dev/mine_transpositions.py")
    
    # 6. Full Decoded Corpus Sources (Priority 7)
    if os.path.exists("ref/morpheus/cubes_decoded.json"):
        z.write("ref/morpheus/cubes_decoded.json", "ref/morpheus/cubes_decoded.json")
    if os.path.exists("ref/stage_vocabulary.json"):
        z.write("ref/stage_vocabulary.json", "ref/stage_vocabulary.json")
    if os.path.exists("recipes/alphabet.json"):
        z.write("recipes/alphabet.json", "recipes/alphabet.json")
        
    # 7. Prefix Operations & Klatt Comparisons (Priority 8)
    if os.path.exists("dev/prefix_ops.py"):
        z.write("dev/prefix_ops.py", "dev/prefix_ops.py")
    if os.path.exists("dev/klatt_compare.py"):
        z.write("dev/klatt_compare.py", "dev/klatt_compare.py")
    if os.path.exists("dev/derive_operation_vocabulary.py"):
        z.write("dev/derive_operation_vocabulary.py", "dev/derive_operation_vocabulary.py")
    if os.path.exists("dev/derive_stage_vocabulary.py"):
        z.write("dev/derive_stage_vocabulary.py", "dev/derive_stage_vocabulary.py")
    if os.path.exists("plots/corpus/prefix_ops.txt"):
        z.write("plots/corpus/prefix_ops.txt", "plots/corpus/prefix_ops.txt")
    if os.path.exists("dev/cluster_corner_operators.py"):
        z.write("dev/cluster_corner_operators.py", "dev/cluster_corner_operators.py")
    if os.path.exists("dev/mine_corner_grammar.py"):
        z.write("dev/mine_corner_grammar.py", "dev/mine_corner_grammar.py")

print(f"SUCCESS: Created comprehensive {zip_path} ({os.path.getsize(zip_path)} bytes)")
