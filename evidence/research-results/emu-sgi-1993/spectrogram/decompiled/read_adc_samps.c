/* read_adc_samps  entry 0x40a728  size 12 bytes */

void read_adc_samps(void)

{
  undefined2 auStack_100c [2050];
  int iStack_8;
  int iStack_4;
  
  iStack_8 = theParms - DAT_1000b974;
  if (DAT_100000d0 != 0) {
    FUN_0042bbc4(port,&x + iStack_8 * 2,DAT_1000b980);
    iStack_4 = 0;
    if (0 < iStack_8) {
      do {
        *(undefined2 *)(&x + iStack_4 * 2) = auStack_100c[iStack_4];
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < iStack_8);
    }
    DAT_1205271c = 0;
    DAT_100000d0 = 0;
  }
  FUN_0040c170(&x + DAT_1205271c * 2,&arry,theParms);
  DAT_1205271c = DAT_1205271c + DAT_1000b974;
  if (DAT_1000b980 + iStack_8 < DAT_1205271c + theParms) {
    DAT_100000d0 = 1;
    iStack_4 = 0;
    if (0 < iStack_8) {
      do {
        auStack_100c[iStack_4] = *(undefined2 *)(&x + (DAT_1205271c + iStack_4) * 2);
        iStack_4 = iStack_4 + 1;
      } while (iStack_4 < iStack_8);
    }
  }
  return;
}

