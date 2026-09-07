/* scale_C  entry 0x40fe4c  size 56 bytes */

void scale_C(int param_1)

{
  float fVar1;
  
  if (DAT_1000b940 == 0) {
    if (param_1 == zscalesl) {
      fVar1 = (float)fl_get_slider_value(param_1);
      DAT_1000acfc = fVar1 * 2.0;
    }
    else if (param_1 == yscalesl) {
      fVar1 = (float)fl_get_slider_value(param_1);
      DAT_1000acf8 = fVar1 * 0.5;
    }
    else if (param_1 == xscalesl) {
      DAT_1000acf4 = fl_get_slider_value(param_1);
    }
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
    }
    FUN_0040d178();
    DAT_1000b940 = 0;
  }
  return;
}

