/* spectrum  entry 0x4065b0  size 12 bytes */

/* WARNING: Instruction at (ram,0x00406708) overlaps instruction at (ram,0x00406704)
    */

void spectrum(undefined4 *param_1,uint param_2,float *param_3,uint param_4)

{
  float *pfVar1;
  undefined4 *puVar2;
  undefined4 *puVar3;
  float *pfVar4;
  float *pfVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar10;
  int iVar11;
  undefined4 uVar12;
  uint uVar9;
  
  iVar10 = 1;
  iVar11 = 0;
  if (1 < (int)param_4) {
    do {
      iVar10 = iVar10 << 1;
      iVar11 = iVar11 + 1;
    } while (iVar10 < (int)param_4);
  }
  if (iVar11 != fftinitialized) {
    FUN_004069ec(iVar11);
    fftinitialized = iVar11;
  }
  puVar2 = &tmpc;
  uVar7 = 0;
  puVar3 = puVar2;
  if (0 < (int)param_2) {
    if ((param_2 & 3) != 0) {
      do {
        uVar12 = *param_1;
        uVar7 = uVar7 + 1;
        puVar3[1] = 0;
        param_1 = param_1 + 1;
        puVar2 = puVar3 + 2;
        *puVar3 = uVar12;
        puVar3 = puVar2;
      } while ((param_2 & 3) != uVar7);
      if (uVar7 == param_2) goto FUN_004066d0;
    }
    do {
      uVar12 = *param_1;
      puVar2[1] = 0;
      *puVar2 = uVar12;
      uVar12 = param_1[1];
      puVar2[3] = 0;
      puVar2[2] = uVar12;
      uVar12 = param_1[2];
      puVar2[5] = 0;
      puVar2[4] = uVar12;
      uVar12 = param_1[3];
      uVar7 = uVar7 + 4;
      puVar2[7] = 0;
      param_1 = param_1 + 4;
      puVar3 = puVar2 + 8;
      puVar2[6] = uVar12;
      puVar2 = puVar3;
    } while (uVar7 != param_2);
  }
FUN_004066d0:
  uVar7 = 0;
  uVar6 = iVar10 - param_2;
  if (((int)param_2 < iVar10) && (0 < (int)uVar6)) {
    if ((uVar6 & 3) == 0) goto FUN_00406708;
    do {
      uVar9 = uVar7;
      uVar8 = uVar9 + 1;
      *puVar3 = 0;
      puVar3[1] = 0;
      puVar3 = puVar3 + 2;
      uVar7 = uVar8;
    } while ((uVar6 & 3) != uVar8);
    uVar7 = uVar9 + 5;
    puVar2 = puVar3;
    if (uVar8 != uVar6) {
      while( true ) {
        puVar2[2] = 0;
        puVar2[3] = 0;
        puVar2[4] = 0;
        puVar2[5] = 0;
        puVar2[6] = 0;
        puVar2[7] = 0;
        puVar3 = puVar2 + 8;
        *puVar2 = 0;
        puVar2[1] = 0;
        if (uVar7 == uVar6) break;
FUN_00406708:
        uVar7 = uVar7 + 4;
        puVar2 = puVar3;
      }
    }
  }
  FUN_00406b88(&tmpc,iVar11);
  FUN_00406c3c(&tmpc,iVar11);
  uVar7 = 0;
  if (0 < (int)param_4) {
    pfVar5 = (float *)&tmpc;
    pfVar4 = (float *)&tmpc;
    if ((param_4 & 3) != 0) {
      do {
        uVar7 = uVar7 + 1;
        pfVar1 = param_3 + 1;
        pfVar5 = pfVar4 + 2;
        *param_3 = *pfVar4 * *pfVar4;
        *param_3 = *param_3 + pfVar4[1] * pfVar4[1];
        param_3 = pfVar1;
        pfVar4 = pfVar5;
      } while ((param_4 & 3) != uVar7);
      if (uVar7 == param_4) {
        return;
      }
    }
    do {
      uVar7 = uVar7 + 4;
      *param_3 = *pfVar5 * *pfVar5;
      *param_3 = *param_3 + pfVar5[1] * pfVar5[1];
      param_3[1] = pfVar5[2] * pfVar5[2];
      param_3[1] = param_3[1] + pfVar5[3] * pfVar5[3];
      param_3[2] = pfVar5[4] * pfVar5[4];
      param_3[2] = param_3[2] + pfVar5[5] * pfVar5[5];
      param_3[3] = pfVar5[6] * pfVar5[6];
      param_3[3] = param_3[3] + pfVar5[7] * pfVar5[7];
      param_3 = param_3 + 4;
      pfVar5 = pfVar5 + 8;
    } while (uVar7 != param_4);
  }
  return;
}

