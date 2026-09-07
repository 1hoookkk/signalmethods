#!/bin/sh
D=/usr/tmp/spec
mkdir -p $D/sounds $D/filt $D/map
cp /CDROM/spectrogram $D/spectrogram
chmod 755 $D/spectrogram
cp /CDROM/sounds/*.aiff $D/sounds/
cd $D
exec ./spectrogram
