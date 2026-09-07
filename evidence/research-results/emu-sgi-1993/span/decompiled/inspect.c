/* inspect  entry 0x406878  size 48 bytes */

void inspect(float *param_1,float *param_2,uint param_3)

{
  int iVar1;
  float *pfVar2;
  uint uVar3;
  int iVar4;
  float fVar5;
  float fVar6;
  
  iVar1 = 1;
  iVar4 = 0;
  if (1 < (int)param_3) {
    do {
      iVar1 = iVar1 << 1;
      iVar4 = iVar4 + 1;
    } while (iVar1 < (int)param_3);
  }
  if (iVar4 != fftinitialized) {
    FUN_004069ec(iVar4);
    fftinitialized = iVar4;
  }
  FUN_00406b88(param_1,iVar4);
  FUN_00406d90(param_1,iVar4);
  uVar3 = 0;
  fVar5 = 1.0 / (float)(int)param_3;
  if (0 < (int)param_3) {
    if ((param_3 & 3) != 0) {
      do {
        fVar6 = *param_1;
        uVar3 = uVar3 + 1;
        pfVar2 = param_2 + 1;
        param_1 = param_1 + 2;
        *param_2 = fVar6 * fVar5;
        param_2 = pfVar2;
      } while ((param_3 & 3) != uVar3);
      if (uVar3 == param_3) {
        return;
      }
    }
    do {
      uVar3 = uVar3 + 4;
      *param_2 = *param_1 * fVar5;
      param_2[1] = param_1[2] * fVar5;
      param_2[2] = param_1[4] * fVar5;
      param_2[3] = param_1[6] * fVar5;
      param_2 = param_2 + 4;
      param_1 = param_1 + 8;
    } while (uVar3 != param_3);
  }
  return;
}

