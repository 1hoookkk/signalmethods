/* gainsl_C  entry 0x40ff94  size 60 bytes */

void gainsl_C(undefined4 param_1)

{
  float fVar1;
  float fVar2;
  
  if (DAT_1000b940 == 0) {
    fVar1 = (float)fl_get_slider_value(param_1);
    DAT_1000ad00 = fVar1 * 40.0;
    fVar1 = log10f((float)largest);
    fVar2 = log10f((float)smallest);
    m = ((double)DAT_1000ad00 * 12.75 - (double)DAT_1000ad04) / (double)(fVar1 - fVar2);
    b = (double)DAT_1000ad04 + m * 6.0;
    if (DAT_1000b90c != 0) {
      DAT_1000b940 = 1;
      FUN_0040d178();
      DAT_1000b940 = 0;
    }
  }
  return;
}

