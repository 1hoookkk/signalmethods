/* winmult  entry 0x4049dc  size 56 bytes */

void winmult(short *param_1,float *param_2,float *param_3,uint param_4)

{
  uint uVar1;
  float *pfVar2;
  short *psVar3;
  float *pfVar4;
  float *pfVar5;
  
  uVar1 = 0;
  if (0 < (int)param_4) {
    pfVar2 = param_3;
    psVar3 = param_1;
    pfVar4 = param_2;
    if ((param_4 & 3) != 0) {
      do {
        uVar1 = uVar1 + 1;
        *pfVar2 = (float)(int)*psVar3 * *pfVar4;
        pfVar2 = pfVar2 + 1;
        psVar3 = psVar3 + 1;
        pfVar4 = pfVar4 + 1;
      } while ((param_4 & 3) != uVar1);
      if (uVar1 == param_4) {
        return;
      }
    }
    pfVar2 = param_3 + uVar1;
    psVar3 = param_1 + uVar1;
    pfVar4 = param_2 + uVar1;
    do {
      pfVar5 = pfVar4 + 4;
      *pfVar2 = (float)(int)*psVar3 * *pfVar4;
      pfVar2[1] = (float)(int)psVar3[1] * pfVar4[1];
      pfVar2[2] = (float)(int)psVar3[2] * pfVar4[2];
      pfVar2[3] = (float)(int)psVar3[3] * pfVar4[3];
      pfVar2 = pfVar2 + 4;
      psVar3 = psVar3 + 4;
      pfVar4 = pfVar5;
    } while (pfVar5 != param_2 + param_4);
  }
  return;
}

