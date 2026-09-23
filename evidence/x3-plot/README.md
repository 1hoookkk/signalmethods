# X3 filter plot, read from EmulatorX.dll in live Ghidra (23 Sep 2026)

Static reading through the ghidra-mcp HTTP bridge (127.0.0.1:8089, program EmulatorX.dll); nothing was executed or captured at runtime.

- `CPhantomFilterPlot` constructor (FUN_180137370) builds a `PhantomFloatTablePlotArtist`: 2000 points, pen colour from DAT_18088e37c = ARGB 0xE6B5FFFF (#B5FFFF at 90 % alpha), pen width int 1 at +0x30, label mode 3.
- Fill (FUN_1801376d0): 8000 response points, each 4 reduced to the largest magnitude, giving 2000 plot points.
- Draw (FUN_18013e8b0, vtable slot 1): GdipCreatePen1(colour, (float) width), GdipSetSmoothingMode(4 = antialias), GdipDrawLines over the 2000 points. No second glow stroke. Optional caption in Tahoma 11, weight 400.
- WM_PAINT (0x180137900, not defined as a function in the project; read by capstone from raw bytes) paints into a memory DC and BitBlts it.
