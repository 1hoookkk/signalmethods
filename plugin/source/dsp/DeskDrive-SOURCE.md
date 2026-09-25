# Output stage source

DeskDrive adapts Chris Johnson's Airwindows Mackity processing:
https://github.com/airwindows/airwindows/blob/master/plugins/WinVST/Mackity/MackityProc.cpp

The process path is Mackity's: input coupling, In Trim, the first low-pass,
the clamp and fifth-order transfer, the second low-pass, output coupling and
Out Pad, with the original coefficients. TRENCH maps the OUTPUT knob onto
In Trim from its unity value (0.1) to full, fixes Out Pad at 1, leaves out the
floating-point dither, and adds exact bypass at OUTPUT 0 plus finite/denormal
guards. The applicable MIT notice is in AIRWINDOWS-LICENSE.txt.
