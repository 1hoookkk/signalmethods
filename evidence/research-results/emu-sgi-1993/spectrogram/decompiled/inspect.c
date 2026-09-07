/* inspect  entry 0x415e98  size 12 bytes */

void inspect(float *param_1,float *param_2,int param_3)

{
  int iVar1;
  float *pfStack_1c;
  float *pfStack_14;
  int iStack_10;
  int iStack_c;
  
  iStack_10 = 1;
  iStack_c = 0;
  if (1 < param_3) {
    do {
      iStack_10 = iStack_10 << 1;
      iStack_c = iStack_c + 1;
    } while (iStack_10 < param_3);
  }
  if (fftinitialized != iStack_c) {
    FUN_00416018(iStack_c);
    fftinitialized = iStack_c;
  }
  FUN_004161b4(param_1,iStack_c);
  FUN_00416548(param_1,iStack_c);
  iVar1 = 0;
  pfStack_1c = param_1;
  pfStack_14 = param_2;
  if (0 < param_3) {
    do {
      *pfStack_14 = *pfStack_1c * (1.0 / (float)param_3);
      pfStack_1c = pfStack_1c + 2;
      pfStack_14 = pfStack_14 + 1;
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}

