/* fft  entry 0x4162e0  size 12 bytes */

void fft(int param_1,uint param_2)

{
  int iVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  int iStack_28;
  uint uStack_24;
  uint uStack_18;
  uint uStack_10;
  uint uStack_4;
  
  iVar3 = *(int *)(&pwr_two + param_2 * 4);
  iVar2 = (&DAT_10000180)[param_2];
  uStack_4 = 1;
  uStack_18 = param_2;
  while (param_2 != 0) {
    uStack_18 = uStack_18 - 1;
    uStack_24 = param_2 - 1;
    uStack_10 = 0;
    iStack_28 = iVar2;
    while (iStack_28 != 0) {
      iStack_28 = iStack_28 + -1;
      uVar4 = uStack_10 << (uStack_18 & 0x1f) & iVar3 - 1U;
      iVar1 = uStack_10 + uStack_4;
      fVar5 = *(float *)(param_1 + iVar1 * 8);
      fVar6 = *(float *)(param_1 + iVar1 * 8 + 4);
      fVar7 = *(float *)(DAT_10000180 + uVar4 * 8);
      fVar8 = *(float *)(DAT_10000180 + uVar4 * 8 + 4);
      fVar9 = fVar5 * fVar7 - fVar6 * fVar8;
      fVar7 = fVar5 * fVar8 + fVar6 * fVar7;
      fVar6 = *(float *)(param_1 + uStack_10 * 8);
      fVar5 = *(float *)(param_1 + uStack_10 * 8 + 4);
      *(float *)(param_1 + iVar1 * 8) = fVar6 - fVar9;
      *(float *)(param_1 + iVar1 * 8 + 4) = fVar5 - fVar7;
      *(float *)(param_1 + uStack_10 * 8) = fVar6 + fVar9;
      *(float *)(param_1 + uStack_10 * 8 + 4) = fVar5 + fVar7;
      uStack_10 = iVar1 + 1U & ~uStack_4;
    }
    uStack_4 = uStack_4 << 1;
    param_2 = uStack_24;
  }
  return;
}

