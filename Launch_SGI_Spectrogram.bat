@echo off
setlocal
set RT=%~dp0evidence\research-results\emu-sgi-1993\runtime
cd /D "%RT%\mame-0.289"
if not exist "cfg\indy_4610.cfg" copy /Y "%RT%\tools\indy_4610.cfg" "cfg\indy_4610.cfg" >nul
echo Booting the emulated SGI Indy (8-bit XL board, 256 MB, sound off) with IRIX 5.3 and the disc attached.
echo.
echo Log in as root, no password. Open a shell, then: sh /CDROM/setup
echo Both programs need a sound file as an argument; spectrogram3d and span1000
echo are the copies patched to open a usable display window.
echo.
echo In the 3D display, HOLD THE MIDDLE BUTTON AND DRAG to tilt it - it opens
echo edge-on and shows nothing until you do. RIGHT button picks menu items.
echo Scroll Lock frees the mouse for MAME's own menu.
echo.
echo Memory comes from cfg\indy_4610.cfg, not -ramsize, which this machine
echo ignores. Check inside IRIX with: hinv -c memory  (must say 256 Mbytes)
mame.exe indy_4610 -gio64_gfx xl8 -hard1 "%RT%\state\irix53-spectrogram.chd" -cdrom "%RT%\spectrogram-sounds.iso" -sound none -window -resolution 1024x768 -nomaximize -mouse
endlocal
