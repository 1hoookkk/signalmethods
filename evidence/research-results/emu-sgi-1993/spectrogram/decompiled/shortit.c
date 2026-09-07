/* shortit  entry 0x40c1c8  size 12 bytes */

void shortit(int param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(short *)(param_2 + iVar1 * 2) = (short)(int)*(float *)(param_1 + iVar1 * 4);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}

