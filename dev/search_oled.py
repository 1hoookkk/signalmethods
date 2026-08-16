import os

for root_dir in ['C:/Users/hooki/trench-authoring', 'C:/Users/hooki/trench-x3-clean']:
    for root, dirs, files in os.walk(root_dir):
        if any(skip in root for skip in ['.git', 'target', 'node_modules']):
            continue
        for f in files:
            if f.endswith(('.rs', '.py', '.c', '.cpp', '.md')):
                p = os.path.join(root, f)
                try:
                    with open(p, 'r', encoding='utf-8', errors='ignore') as fp:
                        txt = fp.read()
                        if 'oled' in txt.lower():
                            print(f"Match 'oled': {p}")
                        if 'wireframe' in txt.lower():
                            print(f"Match 'wireframe': {p}")
                        if 'isometric' in txt.lower():
                            print(f"Match 'isometric': {p}")
                except Exception:
                    pass
