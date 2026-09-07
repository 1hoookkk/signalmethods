from pathlib import Path

import pycdlib


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parent / "spectrogram" / "extracted"
SOUNDS = ROOT / "sounds"
OUTPUT = ROOT / "spectrogram-sounds.iso"

SETUP = """#!/bin/sh
D=/usr/tmp/spec
mkdir -p $D/sounds $D/filt $D/map
cp /CDROM/spectrogram $D/spectrogram
chmod 755 $D/spectrogram
cp /CDROM/sounds/*.aiff $D/sounds/
cd $D
exec ./spectrogram
"""

setup_path = ROOT / "setup.sh"
setup_path.write_text(SETUP, newline="\n")

iso = pycdlib.PyCdlib()
iso.new(
    interchange_level=3,
    sys_ident="SGI",
    vol_ident="SPECTROGRAM",
    app_ident_str="E-MU SPECTROGRAM PRESERVATION PAYLOAD",
    rock_ridge="1.09",
)
iso.add_file(str(SOURCE / "spectrogram"), iso_path="/SPECTROGRAM;1", rr_name="spectrogram", file_mode=0o100755)
iso.add_file(str(SOURCE / "README"), iso_path="/README.;1", rr_name="README", file_mode=0o100644)
iso.add_file(str(setup_path), iso_path="/SETUP.;1", rr_name="setup", file_mode=0o100755)
iso.add_directory("/SOUNDS", rr_name="sounds", file_mode=0o040755)
for i, wav in enumerate(sorted(SOUNDS.glob("*.aiff"))):
    iso.add_file(str(wav), iso_path=f"/SOUNDS/S{i:02d}.AIF;1", rr_name=wav.name, file_mode=0o100644)
iso.write(str(OUTPUT))
iso.close()
print(OUTPUT, OUTPUT.stat().st_size)
