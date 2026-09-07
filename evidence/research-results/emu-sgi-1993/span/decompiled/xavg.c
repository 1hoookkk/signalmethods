/* xavg  entry 0x404cb0  size 12 bytes */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void xavg(float *param_1,int param_2,int param_3)

{
  undefined4 uVar1;
  uint uVar2;
  float *pfVar3;
  float *pfVar4;
  uint uVar5;
  undefined4 *puVar6;
  
  if ((param_3 != 0) && (uVar2 = 0, -1 < param_2)) {
    uVar5 = param_2 + 1U & 3;
    if (uVar5 != 0) {
      puVar6 = &DAT_10007818;
      do {
        uVar2 = uVar2 + 1;
        *puVar6 = 0;
        puVar6 = puVar6 + 1;
      } while (uVar5 != uVar2);
      if (param_2 + 1U == uVar2) goto FUN_00404d40;
    }
    puVar6 = &DAT_10007818 + uVar2;
    do {
      uVar2 = uVar2 + 4;
      puVar6[1] = 0;
      puVar6[2] = 0;
      puVar6[3] = 0;
      *puVar6 = 0;
      puVar6 = puVar6 + 4;
    } while (param_2 + 1U != uVar2);
  }
FUN_00404d40:
  uVar2 = 0;
  if (-1 < param_2) {
    uVar5 = param_2 + 1U & 3;
    if (uVar5 != 0) {
      pfVar3 = (float *)&DAT_10007818;
      pfVar4 = param_1;
      do {
        uVar2 = uVar2 + 1;
        *pfVar4 = (float)((double)*pfVar3 * _expk +
                         (double)(float)((double)*pfVar4 * (double)CONCAT44(expg,DAT_1000001c)));
        *pfVar3 = *pfVar4;
        pfVar3 = pfVar3 + 1;
        pfVar4 = pfVar4 + 1;
      } while (uVar5 != uVar2);
      if (param_2 + 1U == uVar2) {
        return;
      }
    }
    pfVar3 = (float *)(&DAT_10007818 + uVar2);
    pfVar4 = param_1 + uVar2;
    do {
      uVar2 = uVar2 + 4;
      *pfVar4 = (float)((double)*pfVar3 * _expk +
                       (double)(float)((double)*pfVar4 * (double)CONCAT44(expg,DAT_1000001c)));
      uVar1 = DAT_1000001c;
      *pfVar3 = *pfVar4;
      pfVar4[1] = (float)((double)pfVar3[1] * _expk +
                         (double)(float)((double)pfVar4[1] * (double)CONCAT44(expg,uVar1)));
      uVar1 = DAT_1000001c;
      pfVar3[1] = pfVar4[1];
      pfVar4[2] = (float)((double)pfVar3[2] * _expk +
                         (double)(float)((double)pfVar4[2] * (double)CONCAT44(expg,uVar1)));
      uVar1 = DAT_1000001c;
      pfVar3[2] = pfVar4[2];
      pfVar4[3] = (float)((double)pfVar3[3] * _expk +
                         (double)(float)((double)pfVar4[3] * (double)CONCAT44(expg,uVar1)));
      pfVar3[3] = pfVar4[3];
      pfVar3 = pfVar3 + 4;
      pfVar4 = pfVar4 + 4;
    } while (param_2 + 1U != uVar2);
  }
  return;
}

