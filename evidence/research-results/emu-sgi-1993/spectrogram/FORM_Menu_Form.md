# Peevers Spectrogram control panel, from create_form_Menu_Form (FORMS 2.2a)

Form size [344.0, 368.0]. FORMS units, origin bottom-left, y up. Class from the thunk's dispatcher id (21 slider, 11 button, 31 input, 3 menu, 2 text). Attribute calls are listed by thunk address; FUN_00420fa8 sets the callback, the others set colour, label size, style and box type.

| class | type | x | y | w | h | label | var | attributes |
|---|---|---|---|---|---|---|---|---|
| menu | 1 | 10 | 340 | 48.9 | 19.6 | File | FileMenu | c684(9, 0x10); c7b4(0x20); c8fc(2); 0fa8(FileMenu_C, 0) |
| slider | 2 | 10 | 25 | 20 | 160 | R | lRedSlider | c618(6); c684(9, 1); c7b4(1); c820(8.0); 0fa8(lSlider_C, 0) |
| slider | 2 | 30 | 25 | 20 | 160 | G | lGreenSlider | c618(4); c684(10, 2); c7b4(2); c820(8.0); 0fa8(lSlider_C, 0) |
| slider | 2 | 50 | 25 | 20 | 160 | B | lBlueSlider | c618(6); c684(0x10, 4); c7b4(4); c820(8.0); 0fa8(lSlider_C, 0) |
| slider | 2 | 90 | 25 | 20 | 160 | R | hRedSlider | c618(6); c684(9, 1); c7b4(1); c820(8.0); 0fa8(hSlider_C, 0) |
| slider | 2 | 110 | 25 | 20 | 160 | G | hGreenSlider | c618(6); c684(10, 2); c7b4(2); c820(8.0); 0fa8(hSlider_C, 0) |
| slider | 2 | 130 | 25 | 20 | 160 | B | hBlueSlider | c618(6); c684(0x10, 4); c7b4(4); c820(8.0); 0fa8(hSlider_C, 0) |
| menu | 1 | 70 | 340 | 50 | 20 | Colors | MapMenu | c684(9, 0x25); 0fa8(MapMenu_C, 0) |
| slider | 1 | 195 | 115 | 115 | 20 | gain | gainsl | c820(8.0); c8fc(2); 0fa8(gainsl_C, 0) |
| slider | 1 | 195 | 95 | 115 | 20 | floor | floorsl | c820(8.0); c8fc(2); 0fa8(floorsl_C, 0) |
| slider | 1 | 195 | 75 | 115 | 20 | twist | twistsl | c820(8.0); c8fc(2); 0fa8(viewsl_C, 0) |
| button | 0 | 50 | 305 | 40 | 20 | Clear | clearB | c820(8.0); 0fa8(clearit, 0) |
| button | 0 | 10 | 305 | 40 | 20 | LiveIn | freezB | c820(8.0); 0fa8(button_C, 0) |
| input | 0 | 100 | 235 | 50 | 30 | Length | lengthInput | c684(0xc, 6); c820(10.0); c8fc(1); 0fa8(length_C, 0) |
| button | 0 | 90 | 305 | 40 | 20 | Mesh | drawB | c820(8.0); 0fa8(shape_C, 0) |
| slider | 1 | 195 | 135 | 115 | 20 | zoom | fovsl | c820(8.0); c8fc(2); 0fa8(viewsl_C, 0) |
| slider | 1 | 195 | 55 | 115 | 20 | tscale | xscalesl | c820(8.0); c8fc(2); 0fa8(scale_C, 0) |
| slider | 1 | 195 | 155 | 115 | 20 | Cursor | cursorsl | c820(8.0); c8fc(0); 0fa8(cursor_C, 0) |
| slider | 1 | 250 | 239 | 50 | 11 | time | xsl | c618(4); c820(8.0); c8fc(3); 0fa8(viewsl_C, 0) |
| text | 0 | 250 | 250 | 50 | 20 | Translation |  | c820(8.0); c8fc(4) |
| menu | 1 | 60 | 270 | 55 | 20 | Hamm | WinMenu | c684(9, 0x25); 0fa8(WinMenu_C, 0) |
| input | 0 | 60 | 250 | 35 | 15 | FFT Size | orderInput | c618(6); c684(0xc, 6); c820(8.0); 0fa8(Input_C, 0) |
| input | 0 | 60 | 235 | 35 | 15 | Win Size | winsizeInput | c618(6); c684(0xc, 6); c820(8.0); 0fa8(Input_C, 0) |
| input | 0 | 60 | 220 | 35 | 15 | Stride | strideInput | c618(6); c684(0xc, 6); c820(8.0); 0fa8(Input_C, 0) |
| button | 2 | 210 | 265 | 25 | 5 | Monitor | monitorB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 210 | 255 | 25 | 5 | Scope | tgraphB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| slider | 1 | 195 | 35 | 115 | 20 | ascale | yscalesl | c820(8.0); c8fc(2); 0fa8(scale_C, 0) |
| slider | 1 | 195 | 15 | 115 | 20 | fscale | zscalesl | c820(8.0); c8fc(2); 0fa8(scale_C, 0) |
| button | 2 | 244 | 318 | 40 | 20 | ZB | zbB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 245 | 300 | 40 | 20 | DB | dbB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 210 | 245 | 25 | 5 | Meter | bargrphB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| menu | 1 | 130 | 340 | 55 | 20 | Filter | SurfMenu | c684(9, 0x25); 0fa8(SurfMenu_C, 0) |
| button | 2 | 210 | 225 | 25 | 5 | Peaks | partialB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| input | 0 | 310 | 155 | 30 | 20 |  | maxcurInput | c684(0xc, 6); c820(6.0); c8fc(3); 0fa8(cursor_C, 0) |
| input | 0 | 165 | 155 | 30 | 20 |  | mincurInput | c684(0xc, 6); c820(6.0); c8fc(0); 0fa8(cursor_C, 0) |
| button | 2 | 210 | 215 | 25 | 5 | F0 | F0B | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 210 | 235 | 25 | 5 | Surface | specB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 190 | 300 | 40 | 20 | Modify | modB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 245 | 280 | 40 | 20 | LogF | logfreqB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 189 | 318 | 40 | 20 | Impulse | sourceB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 210 | 205 | 25 | 5 | Env | lpcenvB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| slider | 1 | 250 | 225 | 50 | 11 | amp | ysl | c618(4); c820(8.0); c8fc(3); 0fa8(viewsl_C, 0) |
| slider | 1 | 250 | 210 | 50 | 11 | freq | zsl | c618(4); c820(8.0); c8fc(3); 0fa8(viewsl_C, 0) |
| button | 2 | 210 | 195 | 25 | 5 | SynWin | synthwinB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 190 | 280 | 40 | 20 | Persp | perspB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 304 | 280 | 40 | 20 | Axes | axesB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 304 | 318 | 40 | 20 | Polar | polarB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| button | 2 | 304 | 300 | 40 | 20 | 2D | draw3dB | c618(0); c820(8.0); c8fc(2); 0fa8(button_C, 0) |
| text | 0 | 5 | 270 | 40 | 15 | Window |  | c820(8.0); c8fc(4) |
| text | 0 | 5 | 295 | 30 | 5 | Pause |  | c820(8.0) |
