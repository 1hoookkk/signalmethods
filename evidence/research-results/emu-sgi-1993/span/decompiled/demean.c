/* demean  entry 0x404690  size 32 bytes */

void demean(short *param_1,uint param_2)

{
  short *psVar1;
  short *psVar2;
  short sVar3;
  int iVar4;
  uint uVar5;
  short *psVar6;
  short *psVar7;
  uint uVar8;
  float fVar9;
  float fVar10;
  byte in_fcsr;
  
  iVar4 = 0;
  uVar5 = 0;
  if (0 < (int)param_2) {
    psVar6 = param_1;
    if ((param_2 & 3) != 0) {
      do {
        uVar5 = uVar5 + 1;
        iVar4 = iVar4 + *psVar6;
        psVar6 = psVar6 + 1;
      } while ((param_2 & 3) != uVar5);
      if (uVar5 == param_2) goto FUN_00404708;
    }
    psVar6 = param_1 + uVar5;
    do {
      sVar3 = *psVar6;
      psVar7 = psVar6 + 1;
      psVar1 = psVar6 + 2;
      psVar2 = psVar6 + 3;
      psVar6 = psVar6 + 4;
      iVar4 = iVar4 + sVar3 + (int)*psVar7 + (int)*psVar1 + (int)*psVar2;
    } while (psVar6 != param_1 + param_2);
  }
FUN_00404708:
  uVar5 = 0;
  uVar8 = param_2 & 3;
  if (0 < (int)param_2) {
    if (uVar8 != 0) {
      if (param_2 == 0) {
        trap(0x1c00);
      }
      if ((param_2 == 0xffffffff) && (iVar4 == -0x80000000)) {
        trap(0x1800);
      }
      uVar5 = 1;
      fVar9 = (float)(int)*param_1;
      psVar6 = param_1;
      psVar7 = param_1;
      if (uVar8 != 1) {
        do {
          fVar9 = fVar9 - (float)(iVar4 / (int)param_2);
          uVar5 = uVar5 + 1;
          if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
            fVar10 = ROUND(fVar9);
          }
          else {
            fVar10 = FLOOR(fVar9);
          }
          psVar7 = psVar6 + 1;
          fVar9 = (float)(int)psVar6[1];
          *psVar6 = (short)(int)fVar10;
          psVar6 = psVar7;
        } while (uVar8 != uVar5);
      }
      fVar9 = fVar9 - (float)(iVar4 / (int)param_2);
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar9 = ROUND(fVar9);
      }
      else {
        fVar9 = FLOOR(fVar9);
      }
      *psVar7 = (short)(int)fVar9;
      if (uVar5 == param_2) {
        return;
      }
    }
    if (param_2 == 0) {
      trap(0x1c00);
    }
    if ((param_2 == 0xffffffff) && (iVar4 == -0x80000000)) {
      trap(0x1800);
    }
    fVar9 = (float)(iVar4 / (int)param_2);
    sVar3 = param_1[uVar5];
    psVar6 = param_1 + uVar5;
    while( true ) {
      psVar7 = psVar6 + 4;
      if (psVar7 == param_1 + param_2) break;
      fVar10 = (float)(int)sVar3 - fVar9;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND(fVar10);
      }
      else {
        fVar10 = FLOOR(fVar10);
      }
      *psVar6 = (short)(int)fVar10;
      sVar3 = *psVar7;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[1] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[1] - fVar9);
      }
      psVar6[1] = (short)(int)fVar10;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[2] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[2] - fVar9);
      }
      psVar6[2] = (short)(int)fVar10;
      if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
        fVar10 = ROUND((float)(int)psVar6[3] - fVar9);
      }
      else {
        fVar10 = FLOOR((float)(int)psVar6[3] - fVar9);
      }
      psVar6[3] = (short)(int)fVar10;
      psVar6 = psVar7;
    }
    fVar10 = (float)(int)sVar3 - fVar9;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND(fVar10);
    }
    else {
      fVar10 = FLOOR(fVar10);
    }
    *psVar6 = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND((float)(int)psVar6[1] - fVar9);
    }
    else {
      fVar10 = FLOOR((float)(int)psVar6[1] - fVar9);
    }
    psVar6[1] = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar10 = ROUND((float)(int)psVar6[2] - fVar9);
    }
    else {
      fVar10 = FLOOR((float)(int)psVar6[2] - fVar9);
    }
    psVar6[2] = (short)(int)fVar10;
    if ((((in_fcsr | 3) ^ 2) & 3) == 0) {
      fVar9 = ROUND((float)(int)psVar6[3] - fVar9);
    }
    else {
      fVar9 = FLOOR((float)(int)psVar6[3] - fVar9);
    }
    psVar6[3] = (short)(int)fVar9;
  }
  return;
}

