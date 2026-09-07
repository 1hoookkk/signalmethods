/* getmax  entry 0x416b90  size 12 bytes */

float getmax(double *param_1,uint param_2)

{
  uint uVar1;
  float fVar2;
  
  fVar2 = (float)*param_1;
  uVar1 = 0;
  if (param_2 != 0) {
    do {
      if ((double)fVar2 < param_1[uVar1]) {
        fVar2 = (float)param_1[uVar1];
      }
      uVar1 = uVar1 + 1;
    } while (uVar1 < param_2);
  }
  return fVar2;
}

