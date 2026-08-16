import os, glob, json, math
import xml.etree.ElementTree as ET
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

TAU = 2.0 * np.pi
SR_39K = 39062.5
SR_44K = 44100.0

# Let's inspect cross-correlation between P2k presets and Emulator X XML names/structures
emu_root = r'C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter'
xml_files = glob.glob(os.path.join(emu_root, '*.xml'))
p2k_files = glob.glob('ref/presets/*.bin')

p2k_names = [os.path.splitext(os.path.basename(f))[0].lower() for f in p2k_files]
xml_names = [os.path.splitext(os.path.basename(f))[0].lower() for f in xml_files]

print("=== NAME OVERLAPS BETWEEN P2K AND EMULATOR X XML ===")
for xn in xml_names:
    for pn in p2k_names:
        # check partial matches
        core_pn = pn.replace('p2k_', '').replace('_', ' ')
        if core_pn in xn or xn in core_pn:
            print(f"Overlap: XML '{xn}' <-> P2k '{pn}'")

# Generate an overall overview visualization combining all findings:
fig = plt.figure(figsize=(16, 12))
gs = fig.add_gridspec(3, 3, hspace=0.35, wspace=0.28)

# Panel 1: Top Zero States Slot Distribution
ax1 = fig.add_subplot(gs[0, 0])
slots = ['S1', 'S2', 'S3', 'S4', 'S5', 'S6']
nyquist_nulls = [0, 0, 0, 0, 0, 48] # 48 occurrences of 0xFF7D, 0x01F0 on S6
mid_zeros = [4, 7, 14, 15, 13, 0]
ax1.bar(slots, nyquist_nulls, label='Nyquist Null (18kHz r=1.0)', color='#d62728', alpha=0.85)
ax1.bar(slots, mid_zeros, bottom=nyquist_nulls, label='Formant Shaping Zeros', color='#1f77b4', alpha=0.85)
ax1.set_title("Zero State Slot Specificity (P2k Corpus)", fontsize=10, fontweight='bold')
ax1.set_ylabel("Total Occurrences", fontsize=9)
ax1.legend(fontsize=8)
ax1.grid(True, axis='y', alpha=0.3)

# Panel 2: XML Designer Section Slot Pinning
ax2 = fig.add_subplot(gs[0, 1])
xml_slot4_peak = [0, 0, 0, 8, 0, 0]
xml_slot6_notch = [0, 0, 0, 0, 0, 6]
xml_shelf_dist = [4, 3, 3, 3, 3, 1]
ax2.bar(slots, xml_slot4_peak, label='Peak [68,85,109,102] (8 Presets)', color='#2ca02c')
ax2.bar(slots, xml_slot6_notch, bottom=xml_slot4_peak, label='Terminal Notches', color='#9467bd')
ax2.bar(slots, xml_shelf_dist, bottom=[a+b for a,b in zip(xml_slot4_peak, xml_slot6_notch)], label='Other Reused Shelves', color='#ff7f0e')
ax2.set_title("Slot Pinning in Emulator X XML (71 Presets)", fontsize=10, fontweight='bold')
ax2.set_ylabel("Occurrences", fontsize=9)
ax2.legend(fontsize=8)
ax2.grid(True, axis='y', alpha=0.3)

# Panel 3: Recurrence Hierarchy Pie / Doughnut
ax3 = fig.add_subplot(gs[0, 2])
categories = ['Unique Sections', 'Shared Sections (2+)', 'Shared Zero Scaffolds', 'Exact Shared Corners']
counts = [1053, 187, 51, 17]
colors = ['#aec7e8', '#1f77b4', '#ff7f0e', '#d62728']
ax3.pie(counts, labels=categories, colors=colors, autopct='%1.1f%%', startangle=140, textprops={'fontsize': 8})
ax3.set_title("Encoded State Reusability Hierarchy", fontsize=10, fontweight='bold')

# Panel 4: Response comparison of Top 3 Recurring Zeros
ax4 = fig.add_subplot(gs[1, :])
freqs = np.geomspace(40, 18000, 512)
# Zero 1: Nyquist null
z1_b = [1.0, -2.0 * 1.0 * np.cos(2*np.pi*17960.8/SR_39K), 1.0, 0.0, 0.0]
# Zero 2: 2014 Hz zero
z2_b = [1.0, -2.0 * 0.9602 * np.cos(2*np.pi*2014.4/SR_39K), 0.9602**2, 0.0, 0.0]
# Zero 3: 2971 Hz zero
z3_b = [1.0, -2.0 * 0.9723 * np.cos(2*np.pi*2971.3/SR_39K), 0.9723**2, 0.0, 0.0]
# Zero 4: 7921 Hz zero
z4_b = [1.0, -2.0 * 0.7501 * np.cos(2*np.pi*7921.9/SR_39K), 0.7501**2, 0.0, 0.0]

def resp_db(b):
    w = 2.0 * np.pi * freqs / SR_39K
    z1 = np.exp(-1j * w)
    z2 = np.exp(-2j * w)
    num = b[0] + b[1]*z1 + b[2]*z2
    den = 1.0 + b[3]*z1 + b[4]*z2
    return 20.0 * np.log10(np.maximum(np.abs(num/den), 1e-12))

ax4.plot(freqs, resp_db(z1_b), label="S6 Traveling Null: 17.96 kHz, r=1.0 (48 corners in 26 presets)", color='#d62728', lw=2)
ax4.plot(freqs, resp_db(z2_b), label="S3 Vowel Zero: 2014 Hz, r=0.960 (10 corners in 6 presets)", color='#1f77b4', lw=2)
ax4.plot(freqs, resp_db(z3_b), label="S4 Vowel Zero: 2971 Hz, r=0.972 (9 corners in 6 presets)", color='#2ca02c', lw=2)
ax4.plot(freqs, resp_db(z4_b), label="S5 High Shelf Zero: 7922 Hz, r=0.750 (10 corners in 6 presets)", color='#ff7f0e', lw=2)
ax4.set_xscale('log')
ax4.set_title("Frequency Responses of Key Recurring Zero Atoms (at 39.0625 kHz datum)", fontsize=10, fontweight='bold')
ax4.set_xlabel("Frequency (Hz)", fontsize=9)
ax4.set_ylabel("Magnitude (dB)", fontsize=9)
ax4.grid(True, which='both', alpha=0.3)
ax4.legend(loc='lower left', fontsize=8.5)
ax4.set_ylim(-50, 15)

# Panel 5: XML 5-Section Match Response Overlay (Complex BP 1 vs Harmonic LP 5)
ax5 = fig.add_subplot(gs[2, :])
# Plot Harmonic LP 5 vs Complex BP 1 representation
ax5.text(0.5, 0.5, "Complex BP 1 and Harmonic LP 5 share 5 out of 6 Designer Sections bit-for-bit:\n"
                   "• S2: LowShelf [LF=15, HF=33, LG=0, HG=0] (Exact Match)\n"
                   "• S3: LowShelf [LF=33, HF=51, LG=127, HG=127] (Exact Match)\n"
                   "• S4: LowShelf [LF=51, HF=70, LG=0, HG=0] (Exact Match)\n"
                   "• S5: LowShelf [LF=70, HF=89, LG=127, HG=127] (Exact Match)\n"
                   "• S6: Notch    [LF=0,  HF=127, LG=127, HG=127] (Exact Match)\n"
                   "Differ ONLY at S1: Complex BP 1 has [LF=0, HF=15, LG=127, HG=127] vs Harmonic LP 5 has [LF=0, HF=15, LG=0, HG=0]\n\n"
                   "This single bit change flips the fundamental bass shelf from boost to unity across the entire morph trajectory!",
         ha='center', va='center', fontsize=10, bbox=dict(boxstyle='round,pad=0.8', facecolor='#f0f4f8', edgecolor='#1f77b4', lw=1.5))
ax5.axis('off')
ax5.set_title("Case Study: Exact 5-Section Preservation in Emulator X XML", fontsize=10, fontweight='bold')

plt.tight_layout()
plt.savefig('plots/corpus_section_recurrence_summary.png', dpi=200)
plt.close()
print("Saved plots/corpus_section_recurrence_summary.png")
