/* ifft  entry 0x41653c  size 12 bytes */

void ifft(int param_1,uint param_2)

{
  uint uVar1;
  int iVar2;
  float *pfVar3;
  int iVar4;
  int iVar5;
  float fVar6;
  float fVar7;
  int iStack_28;
  uint uStack_24;
  uint uStack_18;
  uint uStack_10;
  uint uStack_4;
  
  iVar5 = *(int *)(&pwr_two + param_2 * 4);
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
      iVar4 = uStack_10 + uStack_4;
      uVar1 = uStack_10 << (uStack_18 & 0x1f) & iVar5 - 1U;
      pfVar3 = (float *)(DAT_10000180 + uVar1 * 8);
      fVar6 = pfVar3[1] * *(float *)(param_1 + iVar4 * 8 + 4) +
              *(float *)(param_1 + iVar4 * 8) * *pfVar3;
      pfVar3 = (float *)(DAT_10000180 + uVar1 * 8);
      fVar7 = *pfVar3 * *(float *)(param_1 + iVar4 * 8 + 4) -
              *(float *)(param_1 + iVar4 * 8) * pfVar3[1];
      *(float *)(param_1 + iVar4 * 8) = *(float *)(param_1 + uStack_10 * 8) - fVar6;
      *(float *)(param_1 + iVar4 * 8 + 4) = *(float *)(param_1 + uStack_10 * 8 + 4) - fVar7;
      *(float *)(param_1 + uStack_10 * 8) = *(float *)(param_1 + uStack_10 * 8) + fVar6;
      *(float *)(param_1 + uStack_10 * 8 + 4) = *(float *)(param_1 + uStack_10 * 8 + 4) + fVar7;
      uStack_10 = iVar4 + 1U & ~uStack_4;
    }
    uStack_4 = uStack_4 << 1;
    param_2 = uStack_24;
  }
  return;
}

