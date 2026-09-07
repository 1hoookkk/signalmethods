/* log_of  entry 0x404af0  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void log_of(float *param_1,int param_2)

{
  float fVar1;
  float *pfVar2;
  float *pfVar3;
  undefined4 uVar4;
  float fVar5;
  undefined4 uVar6;
  float fVar7;
  double dVar8;
  float fVar9;
  
  dVar8 = (double)winsize / _squark;
  fVar5 = log10f((float)order);
  fVar1 = (float)dVar8 / 1.0737418e+09;
  if (power_mode == 0) {
    uVar4 = 0x3ddb7cdf;
    uVar6 = 0xd9d7bdbb;
  }
  else {
    uVar4 = 0x3f1a36e2;
    uVar6 = 0xeb1c432d;
  }
  if (-1 < param_2) {
    fVar9 = (float)((double)CONCAT44(uVar4,uVar6) * (double)(1.0 / fVar1));
    pfVar2 = param_1;
    do {
      fVar7 = *pfVar2;
      if (fVar7 < fVar9) {
        *pfVar2 = fVar9;
        fVar7 = *pfVar2;
      }
      fVar7 = log10f(fVar7 * fVar1);
      pfVar3 = pfVar2 + 1;
      *pfVar2 = fVar7 * 10.0 - fVar5 * 20.0 * (float)power_mode;
      pfVar2 = pfVar3;
    } while (param_1 + param_2 + 1 != pfVar3);
  }
  return;
}

