/* init_flttbl  entry 0x41176c  size 12 bytes */

void init_flttbl(void)

{
  double adStack_2028 [1024];
  void *pvStack_28;
  void *pvStack_24;
  double dStack_20;
  undefined8 uStack_18;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  uStack_18 = 0x3f589374bc6a7efa;
  pvStack_24 = malloc(0x800);
  pvStack_28 = malloc(0x800);
  malloc(0x200);
  malloc(0x1000);
  dStack_20 = (double)sampling_rate;
  iStack_8 = 0;
  if (-1 < DAT_1000acb4) {
    do {
      iStack_c = 0;
      do {
        FUN_00416c84(adStack_2028,DAT_1000b97c,*(undefined4 *)(&foffreq + iStack_c),
                     *(undefined4 *)((int)&foffreq + iStack_c * 8 + 4),0x3ff0000000000000,
                     (&fofbw)[iStack_c],uStack_18,dStack_20);
        (&flttbl)[iStack_8 * 0x400] = (double)(&flttbl)[iStack_8 * 0x400] + adStack_2028[0];
        (&flttbl)[DAT_1000b97c + iStack_8 * 0x400] =
             (double)(&flttbl)[DAT_1000b97c + iStack_8 * 0x400] + adStack_2028[DAT_1000b97c];
        iStack_4 = 1;
        if (1 < DAT_1000b97c) {
          do {
            (&flttbl)[iStack_4 + iStack_8 * 0x400] =
                 (double)(&flttbl)[iStack_4 + iStack_8 * 0x400] + adStack_2028[iStack_4];
            (&flttbl)[(DAT_1000b978 - iStack_4) + iStack_8 * 0x400] =
                 (&flttbl)[iStack_4 + iStack_8 * 0x400];
            iStack_4 = iStack_4 + 1;
          } while (iStack_4 < DAT_1000b97c);
        }
        iStack_c = iStack_c + 1;
      } while (iStack_c < 1);
      foffreq = foffreq - 1.0;
      DAT_10000048 = DAT_10000048 - 1.5;
      DAT_10000050 = DAT_10000050 - 7.0;
      iStack_8 = iStack_8 + 1;
    } while (iStack_8 <= DAT_1000acb4);
  }
  return;
}

