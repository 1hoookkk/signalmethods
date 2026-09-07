/* floatit  entry 0x40c164  size 12 bytes */

void floatit(int param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < param_3) {
    do {
      *(float *)(param_2 + iVar1 * 4) = (float)(int)*(short *)(param_1 + iVar1 * 2);
      iVar1 = iVar1 + 1;
    } while (iVar1 < param_3);
  }
  return;
}

