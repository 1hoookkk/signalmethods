/* fft  entry 0x406c30  size 12 bytes */

void fft(int param_1,int param_2)

{
  bool bVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  int iVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  int iVar9;
  float *pfVar10;
  float *pfVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  float fVar15;
  float fVar16;
  float fVar17;
  
  iVar5 = *(int *)(&pwr_two + param_2 * 4);
  iVar6 = (&DAT_100000e0)[param_2];
  uVar3 = param_2 - 1;
  uVar2 = 1;
  if (param_2 != 0) {
    uVar4 = uVar3;
    do {
      uVar7 = 0;
      if (iVar6 + -1 != -1) {
        iVar8 = iVar6 + -1;
        do {
          iVar9 = uVar7 + uVar2;
          pfVar10 = (float *)(param_1 + iVar9 * 8);
          pfVar11 = (float *)(DAT_100000e0 + (uVar7 << (uVar3 & 0x1f) & iVar5 - 1U) * 8);
          fVar12 = *pfVar10;
          fVar13 = *pfVar11;
          fVar14 = pfVar11[1];
          pfVar11 = (float *)(param_1 + uVar7 * 8);
          fVar16 = *pfVar11;
          fVar17 = pfVar11[1];
          fVar15 = fVar12 * fVar13 - pfVar10[1] * fVar14;
          *pfVar10 = fVar16 - fVar15;
          fVar12 = fVar12 * fVar14 + pfVar10[1] * fVar13;
          uVar7 = iVar9 + 1U & ~uVar2;
          *(float *)(param_1 + iVar9 * 8 + 4) = fVar17 - fVar12;
          *pfVar11 = fVar16 + fVar15;
          pfVar11[1] = fVar17 + fVar12;
          bVar1 = iVar8 != 0;
          iVar8 = iVar8 + -1;
        } while (bVar1);
      }
      uVar2 = uVar2 << 1;
      uVar3 = uVar3 - 1;
      bVar1 = uVar4 != 0;
      uVar4 = uVar4 - 1;
    } while (bVar1);
  }
  return;
}

