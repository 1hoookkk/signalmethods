/* drw_flttbl  entry 0x411a4c  size 12 bytes */

/* WARNING: Heritage AFTER dead removal. Example location: r0x1000acb0 : 0x00411ee4 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */

void drw_flttbl(int param_1,int param_2,int param_3)

{
  int iVar1;
  int iVar2;
  int iStackX_0;
  double adStack_2050 [1024];
  double dStack_50;
  double dStack_48;
  undefined8 uStack_38;
  float fStack_2c;
  float fStack_28;
  float fStack_24;
  int iStack_14;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  int iStack_4;
  
  uStack_38 = 0x3f589374bc6a7efa;
  dStack_50 = (double)sampling_rate;
  if (DAT_1000b978 == 0) {
    trap(0x1c00);
  }
  if ((DAT_1000b978 == -1) && (sampling_rate == -0x80000000)) {
    trap(0x1800);
  }
  dStack_48 = (double)(sampling_rate / DAT_1000b978);
  if (param_3 == 0) {
    DAT_1000ac3c = *(float *)(&freqarry + param_2 * 0xc + param_1 * 4);
    DAT_1000ac38 = param_2;
  }
  else {
    iVar1 = DAT_1000ac38;
    if (param_3 == 1) {
      iStack_14 = param_2 - DAT_1000ac38;
      fStack_2c = *(float *)(&freqarry + param_2 * 0xc + param_1 * 4);
      fStack_28 = fStack_2c - DAT_1000ac3c;
      fStack_24 = fStack_28 / (float)iStack_14;
      iStack_c = 0;
      for (iVar2 = DAT_1000ac38; iStack_10 = param_2, iVar1 = param_2, DAT_1000ac3c = fStack_2c,
          iVar2 <= param_2; iVar2 = iVar2 + 1) {
        *(float *)(&freqarry + iVar2 * 0xc + param_1 * 4) =
             *(float *)(&freqarry + DAT_1000ac38 * 0xc + param_1 * 4) + (float)iStack_c * fStack_24;
        iStack_c = iStack_c + 1;
      }
    }
    DAT_1000ac38 = iVar1;
    iStack_8 = 0;
    if (-1 < DAT_1000acb0) {
      do {
        iStack_4 = 0;
        if (-1 < DAT_1000b97c) {
          do {
            (&flttbl)[iStack_4 + iStack_8 * 0x400] = 0;
            iStack_4 = iStack_4 + 1;
          } while (iStack_4 <= DAT_1000b97c);
        }
        iStackX_0 = 0;
        do {
          if (*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) != 0.0) {
            FUN_00416c84(adStack_2050,DAT_1000b97c + 1,
                         (int)((ulonglong)
                               ((double)*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) *
                               dStack_48) >> 0x20),
                         SUB84((double)*(float *)(&freqarry + iStack_8 * 0xc + iStackX_0 * 4) *
                               dStack_48,0),*(undefined8 *)(&fofamp + iStackX_0 * 8),
                         (&fofbw)[iStackX_0],uStack_38,dStack_50);
            (&flttbl)[iStack_8 * 0x400] = (double)(&flttbl)[iStack_8 * 0x400] + adStack_2050[0];
            (&flttbl)[DAT_1000b97c + iStack_8 * 0x400] =
                 (double)(&flttbl)[DAT_1000b97c + iStack_8 * 0x400] + adStack_2050[DAT_1000b97c];
            iStack_4 = 1;
            if (1 < DAT_1000b97c) {
              do {
                (&flttbl)[iStack_4 + iStack_8 * 0x400] =
                     (double)(&flttbl)[iStack_4 + iStack_8 * 0x400] + adStack_2050[iStack_4];
                (&flttbl)[(DAT_1000b978 - iStack_4) + iStack_8 * 0x400] =
                     (&flttbl)[iStack_4 + iStack_8 * 0x400];
                iStack_4 = iStack_4 + 1;
              } while (iStack_4 < DAT_1000b97c);
            }
          }
          iStackX_0 = iStackX_0 + 1;
        } while (iStackX_0 < 3);
        iStack_8 = iStack_8 + 1;
      } while (iStack_8 <= DAT_1000acb0);
    }
  }
  return;
}

