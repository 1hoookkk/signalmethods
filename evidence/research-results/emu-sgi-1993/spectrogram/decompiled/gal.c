/* gal  entry 0x416f60  size 12 bytes */

undefined4 * gal(undefined4 param_1)

{
  float *pfVar1;
  int iVar2;
  int iStack_4;
  
  iStack_4 = 1;
  DAT_12052828 = param_1;
  do {
    (&DAT_12052828)[iStack_4] =
         *(float *)(iStack_4 * 4 + 0x12052824) -
         *(float *)(iStack_4 * 4 + 0x1205285c) * (float)(&DAT_120527f0)[iStack_4];
    iStack_4 = iStack_4 + 1;
  } while (iStack_4 < 0xd);
  iStack_4 = 0xc;
  do {
    (&DAT_12052860)[iStack_4] =
         *(float *)(iStack_4 * 4 + 0x1205285c) -
         *(float *)(iStack_4 * 4 + 0x12052824) * (float)(&DAT_120527f0)[iStack_4];
    pfVar1 = (float *)(iStack_4 * 4 + 0x12052898);
    *pfVar1 = *pfVar1 * 0.998;
    iVar2 = iStack_4 * 4;
    *(float *)(iVar2 + 0x12052898) =
         *(float *)(iVar2 + 0x12052898) +
         (*(float *)(iVar2 + 0x12052824) * *(float *)(iVar2 + 0x12052824) +
         *(float *)(iVar2 + 0x1205285c) * *(float *)(iVar2 + 0x1205285c)) * 0.002;
    iVar2 = iStack_4 * 4;
    (&DAT_120527f0)[iStack_4] =
         (float)(&DAT_120527f0)[iStack_4] +
         (((float)(&DAT_12052828)[iStack_4] * *(float *)(iVar2 + 0x1205285c) +
          (float)(&DAT_12052860)[iStack_4] * *(float *)(iVar2 + 0x12052824)) * 0.002) /
         *(float *)(iVar2 + 0x12052898);
    iStack_4 = iStack_4 + -1;
  } while (0 < iStack_4);
  DAT_12052860 = param_1;
  DAT_120527f0 = DAT_12052858;
  return &DAT_120527f0;
}

