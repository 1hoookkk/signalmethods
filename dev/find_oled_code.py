import os

root_dirs = ['C:/Users/hooki/trench-authoring', 'C:/Users/hooki/trench-x3-clean', 'C:/Users/hooki']
keywords = ['oled', 'ssd1306', 'sh1106', '128x64', 'display', 'screen_buffer', 'framebuffer', 'draw_cube']

matches = []
for r_dir in ['C:/Users/hooki/trench-authoring', 'C:/Users/hooki/trench-x3-clean']:
    if not os.path.exists(r_dir):
        continue
    for root, dirs, files in os.walk(r_dir):
        if any(skip in root for skip in ['.git', 'target', 'node_modules', '.cargo']):
            continue
        for f in files:
            if f.endswith(('.rs', '.py', '.c', '.h', '.cpp', '.md', '.json')):
                fpath = os.path.join(root, f)
                try:
                    with open(fpath, 'r', encoding='utf-8', errors='ignore') as fp:
                        content = fp.read()
                        for kw in keywords:
                            if kw.lower() in content.lower():
                                matches.append((fpath, kw))
                except Exception:
                    pass

print(f"Found {len(matches)} matches:")
seen = set()
for mp, kw in matches:
    if mp not in seen:
        print(f"  {mp} (matched: {kw})")
        seen.add(mp)
