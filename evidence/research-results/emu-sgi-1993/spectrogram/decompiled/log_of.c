/* log_of  entry 0x40c380  size 12 bytes */

void log_of(int param_1,int param_2)

{
  int iVar1;
  float fVar2;
  
  iVar1 = 0;
  if (-1 < param_2) {
    do {
      if (*(float *)(param_1 + iVar1 * 4) < 1e-05) {
        *(undefined4 *)(param_1 + iVar1 * 4) = 0x3727c5ac;
      }
      fVar2 = log10f(*(float *)(param_1 + iVar1 * 4));
      *(float *)(param_1 + iVar1 * 4) = (float)((double)fVar2 * m + b);
      iVar1 = iVar1 + 1;
    } while (iVar1 <= param_2);
  }
  return;
}

