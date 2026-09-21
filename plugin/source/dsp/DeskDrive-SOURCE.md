# Output stage source

DeskDrive adapts Chris Johnson's Airwindows Mackity processing:
https://github.com/airwindows/airwindows/blob/master/plugins/WinVST/Mackity/MackityProc.cpp

The two low-pass filters, coupling coefficients, and clipped fifth-order
transfer are from Mackity. TRENCH supplies its own knob gain range, compensation,
bypass behavior, and finite/denormal guards. This is not a bit-identical wrapper
of the original plugin. The applicable MIT notice is in AIRWINDOWS-LICENSE.txt.
