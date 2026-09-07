@echo off
setlocal
set RT=%~dp0evidence\research-results\emu-sgi-1993\runtime
cd /D "%RT%\mame-0.289"
echo Booting the emulated SGI Indy (8-bit XL board, 128 MB, sound off) with IRIX 5.3 and the Spectrogram disc attached.
echo Mouse: MIDDLE button drags the view, RIGHT button picks menu items. Scroll Lock frees the mouse for MAME's own menu.
mame.exe indy_4610 -ramsize 128M -gio64_gfx xl8 -hard1 "%RT%\state\irix53-spectrogram.chd" -cdrom "%RT%\spectrogram-sounds.iso" -sound none -window -resolution 1024x768 -nomaximize -mouse
endlocal
