/* viewsl_C  entry 0x40fce8  size 56 bytes */

void viewsl_C(void)

{
  float fVar1;
  
  if ((DAT_1000b940 == 0) && (DAT_1000acc0 != 0)) {
    DAT_1000ace0 = fl_get_slider_value(xsl);
    DAT_1000ace4 = fl_get_slider_value(ysl);
    DAT_1000ace8 = fl_get_slider_value(zsl);
    DAT_1000acec = fl_get_slider_value(fovsl);
    fVar1 = (float)fl_get_slider_value(twistsl);
    DAT_1000acf0 = (fVar1 * 2.0 + -1.0) * 900.0;
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
      FUN_0040d178();
      DAT_1000b940 = 0;
    }
  }
  return;
}

