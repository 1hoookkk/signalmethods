/* spectrum  entry 0x415c10  size 12 bytes */

void spectrum(undefined4 *param_1,int param_2,float *param_3,int param_4)

{
  int iVar1;
  float *pfStack_20;
  float *pfStack_10;
  int iStack_c;
  int iStack_8;
  
  iStack_c = 1;
  iStack_8 = 0;
  if (1 < param_4) {
    do {
      iStack_c = iStack_c << 1;
      iStack_8 = iStack_8 + 1;
    } while (iStack_c < param_4);
  }
  if (fftinitialized != iStack_8) {
    FUN_00416018(iStack_8);
    fftinitialized = iStack_8;
  }
  pfStack_20 = (float *)&tmpc;
  iVar1 = 0;
  pfStack_10 = (float *)param_1;
  if (0 < param_2) {
    do {
      *pfStack_20 = *pfStack_10;
      pfStack_10 = pfStack_10 + 1;
      pfStack_20[1] = 0.0;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_2);
  }
  if ((param_2 < iStack_c) && (iVar1 = 0, 0 < iStack_c - param_2)) {
    do {
      *pfStack_20 = 0.0;
      pfStack_20[1] = 0.0;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < iStack_c - param_2);
  }
  FUN_004161b4(&tmpc,iStack_8);
  FUN_004162ec(&tmpc,iStack_8);
  pfStack_20 = (float *)&tmpc;
  iVar1 = 0;
  pfStack_10 = param_3;
  if (0 < param_4) {
    do {
      *pfStack_10 = *pfStack_20 * *pfStack_20;
      *pfStack_10 = *pfStack_10 + pfStack_20[1] * pfStack_20[1];
      *pfStack_10 = *pfStack_10 / (float)(param_4 * param_4);
      pfStack_10 = pfStack_10 + 1;
      pfStack_20 = pfStack_20 + 2;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_4);
  }
  return;
}

