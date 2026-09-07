/* normalize  entry 0x416c10  size 12 bytes */

void normalize(int param_1,uint param_2,float param_3)

{
  uint uVar1;
  
  uVar1 = 0;
  if (param_2 != 0) {
    do {
      *(double *)(param_1 + uVar1 * 8) = *(double *)(param_1 + uVar1 * 8) * (double)param_3;
      uVar1 = uVar1 + 1;
    } while (uVar1 < param_2);
  }
  return;
}

