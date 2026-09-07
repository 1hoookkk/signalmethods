/* linmult  entry 0x40c0ac  size 32 bytes */

void linmult(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar1 = param_2;
  if (param_2 < 0) {
    iVar1 = param_2 + 1;
  }
  iVar2 = 0;
  if (0 < param_2) {
    do {
      *(float *)(param_1 + iVar2 * 4) =
           *(float *)(param_1 + iVar2 * 4) *
           (1.0 - ABS((float)iVar2 - (float)(iVar1 >> 1)) / (float)(iVar1 >> 1));
      iVar2 = iVar2 + 1;
    } while (iVar2 < param_2);
  }
  return;
}

